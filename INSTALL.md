# Installing SC4 Politics (no programming needed)

You need: SimCity 4 Deluxe, the **digital 1.1.641 version** (GOG/Steam/Origin) on Windows, and a free GitHub account.
GitHub compiles the plugin for you; you only click buttons.

## Part 1 - get the plugin file (about 10 minutes, once)
1. Sign in at github.com. Click **+** (top right) > **New repository**. Name it `sc4-politics`, choose **Public**, click **Create repository**.
2. Unzip `sc4-politics.zip`. On the new repo page click **uploading an existing file**, drag in everything INSIDE the unzipped folder (`CMakeLists.txt`, `README.md`, `include`, `plugin`, `config`, `docs`, `tests`, `tools`), then click **Commit changes**.
3. Click **Add file > Create new file**. In the name box type `.github/workflows/build.yml` (typing the slashes makes the folders). Paste in the contents of the file `github-workflow-build.yml` from the zip. Click **Commit changes**.
4. Click the **Actions** tab. A run called "Build SC4Politics plugin" starts by itself. Wait for a green tick (3-5 minutes).
   - If it shows a red cross, open it, copy the red error text and send it to me. First builds often need a small fix.
5. Click the finished run, scroll to **Artifacts**, download **SC4Politics**, unzip it. You get `SC4Politics.dll` and `Politics.ini`.

## Part 2 - install
1. Close SimCity 4.
2. Copy both files into `Documents\SimCity 4\Plugins`.
3. Start SimCity 4, open a city and let it run for one in-game month.
4. In a web browser (second monitor, or Alt-Tab) open **http://127.0.0.1:9031/**. You will see the party support graph and the City Council.

A log is written next to the DLL as `SC4Politics.log`. If nothing appears, send me that file.

## Changing things
Edit `Politics.ini` (Notepad) and restart the game: council size, party weights, the assumed age mix, the port.

## Uninstall
Delete `SC4Politics.dll` and `Politics.ini` from the Plugins folder. Nothing is written to your save games.
