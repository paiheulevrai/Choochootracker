package com.paiheulevrai.choochootracker;

import android.content.ContentResolver;
import android.database.Cursor;
import android.net.Uri;
import android.os.ParcelFileDescriptor;
import android.provider.DocumentsContract;
import android.provider.DocumentsContract.Document;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

// The user's workspace folder, picked with ACTION_OPEN_DOCUMENT_TREE and
// accessed through DocumentsContract. Paths are relative to the picked folder
// ("" is the folder itself, "projects/a.cct" a file in it). The native engine
// reaches this through the saf* methods of ChooChooTrackerActivity.
//
// Directory listings are cached for a short time so that the many stat() calls
// of a folder listing cost one provider query per folder, and every change made
// here invalidates what it touches.
final class SafWorkspace {
    private static final long LISTING_TTL_MS = 2000;

    private static final class Node {
        final String docId;
        final boolean dir;
        final long size;
        final long mtime;

        Node(String docId, boolean dir, long size, long mtime) {
            this.docId = docId;
            this.dir = dir;
            this.size = size;
            this.mtime = mtime;
        }
    }

    private static final class Listing {
        final Map<String, Node> children;
        final long loadedAt = System.currentTimeMillis();

        Listing(Map<String, Node> children) {
            this.children = children;
        }

        boolean fresh() {
            return System.currentTimeMillis() - loadedAt < LISTING_TTL_MS;
        }
    }

    private final ContentResolver resolver;
    private final Uri tree;
    private final Node root;
    private final HashMap<String, Listing> listings = new HashMap<>();

    SafWorkspace(ContentResolver resolver, Uri tree) {
        this.resolver = resolver;
        this.tree = tree;
        this.root = new Node(DocumentsContract.getTreeDocumentId(tree), true, 0, 0);
    }

    private Uri documentUri(Node node) {
        return DocumentsContract.buildDocumentUriUsingTree(tree, node.docId);
    }

    private static String parentOf(String path) {
        int slash = path.lastIndexOf('/');
        return slash < 0 ? "" : path.substring(0, slash);
    }

    private static String nameOf(String path) {
        return path.substring(path.lastIndexOf('/') + 1);
    }

    private Listing listing(String dirPath, Node dir) {
        Listing cached = listings.get(dirPath);
        if (cached != null && cached.fresh()) return cached;
        Map<String, Node> children = new HashMap<>();
        Uri childrenUri = DocumentsContract.buildChildDocumentsUriUsingTree(tree, dir.docId);
        try (Cursor cursor = resolver.query(childrenUri, new String[] {
                Document.COLUMN_DOCUMENT_ID, Document.COLUMN_DISPLAY_NAME, Document.COLUMN_MIME_TYPE,
                Document.COLUMN_SIZE, Document.COLUMN_LAST_MODIFIED }, null, null, null)) {
            while (cursor != null && cursor.moveToNext()) {
                String name = cursor.getString(1);
                if (name == null) continue;
                boolean isDir = Document.MIME_TYPE_DIR.equals(cursor.getString(2));
                children.put(name, new Node(cursor.getString(0), isDir,
                    cursor.isNull(3) ? 0 : cursor.getLong(3), cursor.isNull(4) ? 0 : cursor.getLong(4)));
            }
        } catch (Exception e) {
            android.util.Log.w("CCTSaf", "listing failed for '" + dirPath + "'", e);
        }
        Listing result = new Listing(children);
        listings.put(dirPath, result);
        return result;
    }

    private Node resolve(String path) {
        if (path.isEmpty()) return root;
        Node parent = resolve(parentOf(path));
        if (parent == null || !parent.dir) return null;
        return listing(parentOf(path), parent).children.get(nameOf(path));
    }

    private Node createChild(String parentPath, Node parent, String name, String mime) {
        try {
            Uri created = DocumentsContract.createDocument(resolver, documentUri(parent), mime, name);
            listings.remove(parentPath);
            if (created == null) return null;
            return new Node(DocumentsContract.getDocumentId(created), Document.MIME_TYPE_DIR.equals(mime), 0, 0);
        } catch (Exception e) {
            android.util.Log.w("CCTSaf", "create failed for '" + name + "'", e);
            return null;
        }
    }

    // Creates the folder and any missing parents; returns it, or null on failure.
    private Node makeDirs(String path) {
        if (path.isEmpty()) return root;
        Node existing = resolve(path);
        if (existing != null) return existing.dir ? existing : null;
        Node parent = makeDirs(parentOf(path));
        if (parent == null) return null;
        return createChild(parentOf(path), parent, nameOf(path), Document.MIME_TYPE_DIR);
    }

    // Returns a detached file descriptor the native side owns, or -1.
    synchronized int open(String path, String mode) {
        boolean write = !mode.equals("r");
        try {
            Node node = resolve(path);
            if (node != null && node.dir) return -1;
            if (node == null) {
                if (!write) return -1;
                Node parent = makeDirs(parentOf(path));
                if (parent == null) return -1;
                node = createChild(parentOf(path), parent, nameOf(path), "application/octet-stream");
                if (node == null) return -1;
            } else if (write) {
                listings.remove(parentOf(path));  // size and time are about to change
            }
            ParcelFileDescriptor pfd = resolver.openFileDescriptor(documentUri(node), mode);
            return pfd == null ? -1 : pfd.detachFd();
        } catch (Exception e) {
            android.util.Log.w("CCTSaf", "open failed for '" + path + "' (" + mode + ")", e);
            return -1;
        }
    }

    // {isDir, size, lastModifiedMs}, or null when the path does not exist.
    synchronized long[] stat(String path) {
        Node node = resolve(path);
        if (node == null) return null;
        return new long[] { node.dir ? 1 : 0, node.size, node.mtime };
    }

    // "d<name>" for folders, "f<name>" for files; null when not a folder.
    synchronized String[] list(String path) {
        Node node = resolve(path);
        if (node == null || !node.dir) return null;
        List<String> names = new ArrayList<>();
        for (Map.Entry<String, Node> entry : listing(path, node).children.entrySet())
            names.add((entry.getValue().dir ? "d" : "f") + entry.getKey());
        return names.toArray(new String[0]);
    }

    // Paths of every file below `path`, relative to the picked folder.
    synchronized void collectFiles(String path, List<String> out) {
        String[] names = list(path);
        if (names == null) return;
        for (String name : names) {
            String child = path.isEmpty() ? name.substring(1) : path + "/" + name.substring(1);
            if (name.charAt(0) == 'd') collectFiles(child, out);
            else out.add(child);
        }
    }

    // 0 created, 1 already a folder, -1 failure.
    synchronized int mkdir(String path) {
        Node existing = resolve(path);
        if (existing != null) return existing.dir ? 1 : -1;
        return makeDirs(path) != null ? 0 : -1;
    }

    synchronized int delete(String path) {
        Node node = resolve(path);
        if (node == null || path.isEmpty()) return -1;
        try {
            boolean deleted = DocumentsContract.deleteDocument(resolver, documentUri(node));
            listings.clear();
            return deleted ? 0 : -1;
        } catch (Exception e) {
            android.util.Log.w("CCTSaf", "delete failed for '" + path + "'", e);
            return -1;
        }
    }

    // POSIX-like rename: an existing target is replaced.
    synchronized int rename(String from, String to) {
        Node source = resolve(from);
        if (source == null || from.isEmpty() || to.isEmpty()) return -1;
        if (from.equals(to)) return 0;
        try {
            Node target = resolve(to);
            if (target != null) {
                if (target.dir != source.dir) return -1;
                if (delete(to) != 0) return -1;
            }
            String fromParent = parentOf(from);
            String toParent = parentOf(to);
            Uri current = documentUri(source);
            if (!fromParent.equals(toParent)) {
                Node newParent = makeDirs(toParent);
                Node oldParent = resolve(fromParent);
                if (newParent == null || oldParent == null) return -1;
                current = DocumentsContract.moveDocument(resolver, current, documentUri(oldParent), documentUri(newParent));
                if (current == null) return -1;
            }
            if (!nameOf(from).equals(nameOf(to))) {
                current = DocumentsContract.renameDocument(resolver, current, nameOf(to));
                if (current == null) return -1;
            }
            listings.clear();
            return 0;
        } catch (Exception e) {
            android.util.Log.w("CCTSaf", "rename failed '" + from + "' -> '" + to + "'", e);
            listings.clear();
            return -1;
        }
    }
}
