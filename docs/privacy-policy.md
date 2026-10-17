# ChooChooTracker privacy policy

Effective date: September 10, 2026

ChooChooTracker does not collect, receive, transmit, sell, or share personal data. The app has no account system, advertising, or analytics.

**Ableton Link (opt-in).** When the user selects Ableton Link on the Project screen and presses Connect, the app joins a local Link session via UDP multicast on the local network (LAN). This is opt-in network activity: it does not run unless the user explicitly enables it. The app acts as a receiver only — it follows the session tempo, never pushes tempo or transport to other participants. No data is sent to remote endpoints; multicast packets stay on the LAN. Link is disabled by default. On Android, the app requests the INTERNET, ACCESS_WIFI_STATE, ACCESS_NETWORK_STATE, and CHANGE_WIFI_MULTICAST_STATE permissions; these are used solely for Link session discovery and have no other purpose.

Projects, samples, settings, and exports are stored only on the user's device. They are used by the app to open, save, and play music. The publisher does not have access to them.

On Android, ChooChooTracker uses the system file picker only when the user chooses to import or export a file. The app does not access files outside that action.

For privacy questions, open an issue in the project repository.
