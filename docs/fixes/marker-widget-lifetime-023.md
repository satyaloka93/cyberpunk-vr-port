# Reused widget addresses must not flatten quest markers

Source: dariulone/cyberpunk-vr-port 0.2.3, e91394502e (2026-11-08).

Exact upstream WorldMarkers.cpp patch. A cached marker entry is usable only if
its weak handle still resolves to the widget being painted. A destroyed root's
address may become a child of another marker; treating it as the old root resets
inherited depth and produces a second flat quest marker. Validate ancestor
entries as well. Prune expired/replaced entries once per second, retaining live
hidden roots and their scale ownership.

Validation: test_widget_lifetime.py executes the production paint lookup and
pruning blocks against expired/reused addresses, mismatched handles, live hidden
roots and inherited depth. MSVC Release builds. In-headset validation pending.
