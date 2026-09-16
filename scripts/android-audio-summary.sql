-- Run with the official Perfetto trace_processor:
-- python .tmp/trace_processor query -f scripts/android-audio-summary.sql TRACE
SELECT name, value FROM stats WHERE severity = 'error' AND value > 0;

SELECT t.tid, t.name, s.state, COUNT(*) AS intervals,
       ROUND(MAX(s.dur) / 1e6, 3) AS max_ms,
       ROUND(AVG(s.dur) / 1e6, 3) AS mean_ms
FROM thread_state s JOIN thread t USING (utid)
WHERE t.name IN ('SDLAudioP1', 'AudioTrack', 'FastMixer') AND s.dur > 0
GROUP BY t.utid, s.state;

SELECT ROUND((s.ts - b.start_ts) / 1e9, 3) AS seconds,
       t.tid, t.name AS thread, s.name AS event
FROM slice s JOIN thread_track tt ON tt.id = s.track_id
JOIN thread t USING (utid), trace_bounds b
WHERE s.name = 'underrun' ORDER BY s.ts;

SELECT ct.name, MIN(c.value) AS minimum, MAX(c.value) AS maximum
FROM counter c JOIN counter_track ct ON ct.id = c.track_id
WHERE ct.name GLOB 'audio.track.fRdy*' GROUP BY c.track_id;
