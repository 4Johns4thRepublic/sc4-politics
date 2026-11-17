# Limitations and honest status

## What exists
- Scoring, 100.0% normalisation, council apportionment, INI config: unit-tested (`tests/test_engine.cpp`).
- `plugin/src/PoliticsDirector.cpp`: a GZCOM plugin written against the real gzcom-dll headers
  (commit 74a527c) and modelled on two working plugins. It parses against those headers
  (checked with clang using stub Windows headers; only 64-bit-vs-32-bit size asserts fail in that check).
- GitHub Actions workflow that builds the 32-bit DLL.

## NOT verified (nobody has run this in SimCity 4 yet)
1. **It has never been compiled with MSVC or loaded by the game.** The first CI build may need small fixes.
2. **Display is a browser page, not an in-game tab.** A new "City Council" tab inside the game's own UI
   is not implemented; it is the riskiest requirement and needs separate research.
3. **Where each number comes from**
   | Trait | Source | Confidence |
   |---|---|---|
   | Low/Med/High density | Zoned residential tile counts (`cISC4ZoneManager::GetZoneCount`) | good |
   | R1/R2/R3, Mid/High income | R$/R$$/R$$$ population (demand ids 0x1010/20/30) | good; R1-3 meaning is assumed |
   | CO$$ | Share of commercial jobs (ids 0x3110-0x3330) | fair |
   | I-Ag/I-D/I-HT | Share of industrial jobs (ids 0x4100/0x4200/0x4400) | fair: 0x4100 is assumed to be agriculture |
   | High education | City average EQ / max EQ | proxy only: an average, not a count above a threshold |
   | **Age brackets** | **Not available from the game.** Fixed shares from `[Assumed]` in Politics.ini | assumed, same in every city |
4. Because ages are assumed, age traits shift every city's result by the same amount. Parties' age-based
   preferences therefore have no effect on how results *change* as a city develops.
5. The month counter in the web view counts months since the game was loaded, not the in-game date.

## Platform
Digital SimCity 4 v1.1.641 on Windows (32-bit DLL). Disc versions and macOS are not supported.
The plugin listens on 127.0.0.1 only (this computer), port 9031 by default.
Saves are never modified; results are recomputed each month.

## Spec choices (editable)
- `+`/`++` = weight 1/2. "Age 31-51" = 31-50, "Age 20-30" = 21-30. R1/R2/R3 = R$/R$$/R$$$.
- Scores are weighted averages by default (otherwise New Liberal wins by listing 11 traits).
- Council: 25 seats, largest-remainder.
