# Bike controls backport

The maintained 0.2.3 fork uses the handlebar and body-lean steering math from
upstream 0.2.45 (`6f33af62`), without its vehicle minimap or new rim-grab system.
Existing vehicle camera offsets, road haptics and car grip behavior are retained.

Under F10 → Controls → Driving → Bike:

- **Toggle grip** defaults **off**. Squeeze near a handlebar and hold to retain
  that hand. When enabled, squeeze once to attach, release your fingers, and
  squeeze again to detach. Each hand works independently.
- **Bike full lock at (deg)** defaults to **40°**, adjustable from 15–60°.
  Push one hand forward and pull the other back; the bars turn in the horizontal
  plane. One hand also works. Changing hands preserves the current turn.
- **Bike body lean** defaults to **0.50**. A sideways lean of 15 cm adds half
  steering lock; 0 disables this contribution. The first 3 cm are a deadzone.
  Its neutral position is captured when you grab the bars. Neck-position
  estimation avoids treating simply looking sideways as a lean.

These values persist as `xr_bike_grip_toggle`, `xr_bike_bar_lock_deg` and
`xr_bike_body_lean` in `config/vrport.ini`. Settings apply live. Turning toggle
mode off releases an existing latch when the grip is open. Weapon ownership,
tracking loss and leaving the vehicle still release the appropriate hand.

Validation: existing numerical car/steering/prediction cases, upstream handlebar
and neck-lean cases, and production grip-state blocks for hold/toggle modes and
camera-miss release. In-headset validation of the newly backported bike behavior
is still required; the scanner fix has already been user-validated separately.
