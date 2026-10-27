# 2.50 regression checks

Run from any directory with Python 3 and `lupa` installed:

```sh
python3 tools/game250_regression_tests/test_reload_cleanup.py
python3 tools/game250_regression_tests/test_crane_rest.py
python3 tools/game250_regression_tests/test_revolver_exit.py
g++ -std=c++17 -pthread -I include tools/game250_regression_tests/hand_muzzle.cpp -o /tmp/cpvr-hand-muzzle
/tmp/cpvr-hand-muzzle
```

The C++ command assumes repository-root working directory and a C++17 compiler.
Lua checks execute production code with mocked game services. They verify reload
ownership cleanup, stable crane pose, level round retention and completed assisted
extraction at 30/60/90 Hz. The C++ fixture checks independent weapon identity,
direction, stale samples and invalid quaternion rejection. These cannot establish
in-game mesh alignment, engine ABI correctness or visual comfort.
