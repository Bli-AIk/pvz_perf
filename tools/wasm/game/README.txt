pvz_perf Web

Windows: extract the whole ZIP, then run start-local.cmd (requires Python 3).
Linux/macOS: run sh start-local.sh (requires Python 3).
The browser opens http://127.0.0.1:18088/. Keep the terminal open while playing.
If the port is in use: start-local.cmd --port 18089
To allow phones on the same LAN: start-local.cmd --host 0.0.0.0
Then use the computer's LAN IP; allow this port in the firewall if necessary.

Do not open index.html with file://: browser security blocks resource loading.
With bypassBilibiliChecks=true, the game, audio and save buttons work without Internet.

web-config.js is the ONLY difference between the GitHub and Bilibili packages:
  bypassBilibiliChecks: true  = local/offline, no Bilibili API calls.
  bypassBilibiliChecks: false = check Bilibili actions on claiming saves.
  rewardVideoBvids: []        = editable BV list; any coin on any listed video qualifies.
After editing configuration, refresh the page. No Wasm rebuild is necessary.
An empty BV list does not grant the perfect save when bypass is disabled.
This is a convenience gate, not encryption: all template saves are bundled.

Save claims replace the local PerfTest profile. Endless import also replaces its
Pool Endless checkpoint. The profile includes Wall-nut First Aid.
Progress is kept in this browser's IndexedDB. Quit through the game's menu to save.
Use the same hostname and port to keep accessing the same browser save storage.

The current endless template is the canonical Pool Endless benchmark.
Additional classic formations require separate compatible source saves.
Licenses for the Web music decoder are under audio/licenses/.
