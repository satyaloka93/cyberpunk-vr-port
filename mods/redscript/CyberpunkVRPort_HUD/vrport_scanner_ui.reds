// CyberpunkVRPort — move the complete scanner/quickhack information panel into the VR view.
//
// Controller/widget identification credited to nben/Cyberpunk-UI-mods-for-VR:
// https://github.com/nben/Cyberpunk-UI-mods-for-VR/tree/main/ScannerUIMove
//
// Keep this as a small, one-shot redscript wrapper. Unlike the earlier CET experiment, it does
// not retain widget references or rewrite an asynchronously spawned widget every 100 ms.

module CyberpunkVRPort.Hud

@wrapMethod(scannerDetailsGameController)
protected cb func OnInitialize() -> Bool {
  let result = wrappedMethod();
  let root = this.GetRootWidget();

  if IsDefined(root) {
    // Negative X moves the right-side information popup toward the center of the view.
    root.SetMargin(new inkMargin(-500.0, 0.0, 0.0, 0.0));
  }

  return result;
}
