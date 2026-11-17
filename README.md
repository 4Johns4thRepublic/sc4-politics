# SC4 Politics

Five parties, support computed from your city's real data each in-game month, shown as a graph and a City Council
in a browser window next to the game. **Start with INSTALL.md.** Read docs/LIMITATIONS.md for what is assumed.

- `include/politics/` scoring engine (portable C++17, tested)
- `plugin/` the SimCity 4 plugin (Windows, 32-bit) and its web page
- `config/Politics.ini` all settings
- `tools/gen_webpage.py` regenerates `plugin/src/WebPage.h` after editing `plugin/web/index.html`

Test the engine: `g++ -std=c++17 -Iinclude tests/test_engine.cpp -o t && ./t`
