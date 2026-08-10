// CyberpunkVRPort — move UI elements that are not reliable inkHUDLayer root children.
//
// Controller/widget identification and initial VR-safe coordinates credited to:
// https://github.com/nben/Cyberpunk-UI-mods-for-VR
//
// These are one-shot initialization wrappers. They do not retain widget references or poll.

module CyberpunkVRPort.Hud

@wrapMethod(PhoneDialerLogicController)
protected cb func OnInitialize() -> Bool {
  let result = wrappedMethod();
  let root = this.GetRootWidget();

  if IsDefined(root) {
    root.SetMargin(new inkMargin(1150.0, 650.0, 0.0, 0.0));
  }

  return result;
}

@wrapMethod(PhoneMessagePopupGameController)
protected cb func OnInitialize() -> Bool {
  let result = wrappedMethod();
  let root = this.GetRootWidget();

  if IsDefined(root) {
    root.SetMargin(new inkMargin(1150.0, 850.0, 0.0, 0.0));
  }

  return result;
}

@wrapMethod(GenericNotificationController)
protected cb func OnInitialize() -> Bool {
  let result = wrappedMethod();
  let root = this.GetRootWidget();

  if IsDefined(root) {
    root.SetMargin(new inkMargin(1150.0, 0.0, 0.0, 0.0));
  }

  return result;
}

@wrapMethod(TutorialPopupGameController)
protected cb func OnInitialize() -> Bool {
  let result = wrappedMethod();
  let root = this.GetRootWidget();

  if IsDefined(root) {
    root.SetMargin(new inkMargin(550.0, 0.0, 0.0, 0.0));
  }

  return result;
}
