"""Keep native adaptive effects intact while checking rate-timed grip pulses."""
from pathlib import Path
import unittest
from lupa import LuaRuntime

SOURCE = Path(__file__).resolve().parents[2] / 'patches/dualsense/PSVR2AutoFireGrip.lua'


class AutoFireGripTests(unittest.TestCase):
    def setUp(self):
        self.lua = LuaRuntime(unpack_returned_tuples=True)
        self.apply = self.lua.execute(SOURCE.read_text(encoding='utf-8'))
        self.pulses = []
        self.lua.globals().record_pulse = lambda *args: self.pulses.append(args)
        self.publisher = self.lua.eval('function(...) record_pulse(...) end')

    def frame(self, effect=None, weapon='Wea_Rifle', state='Shoot', mode='FullAuto',
              rate=8, name='rifle', delta=0.02, melee=False, cyberware=False,
              enabled=True):
        if effect is None:
            effect = self.lua.table_from({'rightTriggerType': 'Bow',
                                          'rightForceTrigger': '(0)(3)(5)(5)'})
        self.apply(effect, weapon, state, mode, rate, name, delta, melee,
                   cyberware, enabled, self.publisher)
        return effect

    def test_keeps_adaptive_profile_byte_values(self):
        effect = self.frame()
        self.assertEqual((effect['rightTriggerType'], effect['rightForceTrigger']),
                         ('Bow', '(0)(3)(5)(5)'))
        self.assertEqual(self.pulses, [(1, 0.68, 36)])

    def test_rate_timed_not_every_update(self):
        for _ in range(10):
            self.frame()
        self.assertEqual(len(self.pulses), 2)

    def test_existing_rhythm_is_not_doubled(self):
        for mode in ('Machine', 'AutomaticGun'):
            self.frame(effect=self.lua.table_from({'rightTriggerType': mode}))
        self.assertEqual(self.pulses, [])

    def test_non_auto_and_non_firing_gates(self):
        for mode in ('SemiAuto', 'Burst'):
            self.frame(mode=mode)
        for state in ('Charging', 'Reload', 'NoAmmo', 'Ready'):
            self.frame(state=state, mode='Charge')
        self.frame(rate=5)
        self.frame(weapon='Wea_Katana')
        self.frame(melee=True)
        self.frame(cyberware=True)
        self.frame(enabled=False)
        self.assertEqual(self.pulses, [])

    def test_tech_charge_discharge_and_rearm(self):
        self.frame(weapon='Wea_SubmachineGun', mode='Charge', name='tech')
        self.frame(state='Charging', mode='Charge', name='tech')
        self.frame(weapon='Wea_SubmachineGun', mode='Charge', name='tech')
        self.assertEqual(len(self.pulses), 2)


if __name__ == '__main__':
    unittest.main()
