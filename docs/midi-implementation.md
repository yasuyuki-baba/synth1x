# MIDI Implementation Chart

Synth1X receives MIDI 1.0 channel messages on channels 1-16. Channel messages
control one shared instrument in Synth1-compatible performance mode. Synth1X
sends no MIDI output, and the arpeggiator plays the internal synth.

| Function | Transmitted | Recognized | Behavior |
| --- | --- | --- | --- |
| Basic channel | No | 1-16 | Omni in Synth1-compatible performance mode |
| Note On and Note Off | No | Yes | Note numbers 0-127. Note On with velocity 0 acts as Note Off |
| Note On velocity | No | Yes | Velocities 1-127 affect the voice according to the sound |
| Note Off velocity | No | No | Release follows the sound's envelope |
| Polyphonic key pressure | No | No | |
| Channel pressure (aftertouch) | No | Yes | Assignable to either MIDI route |
| Pitch bend | No | Yes | 14-bit input, with a per-sound bend range and optional MIDI routing |
| CC1-11 | No | Yes | Assignable to either MIDI route |
| CC7 Channel Volume | No | Yes | Also controls output volume independently of the two routes and Master volume. Initial value is 64 |
| CC64 Sustain | No | Yes | Values 64-127 engage the pedal |
| CC66 Sostenuto | No | Partial | Voice-level pedal handling, with no arpeggiator support |
| CC0 / CC32 Bank Select | No | No | Program Change uses one bank |
| Program Change | No | 0-127 | Recalls an assigned Library sound. An unassigned slot leaves the current sound selected |
| CC120 All Sound Off / CC123 All Notes Off | No | Yes | Both clear the shared keyboard and release voices through the envelope, matching Synth1 behavior |
| CC121 Reset All Controllers | No | Partial | Centers ordinary pitch bend. Pedal and MIDI route values are retained |
| Other CCs, RPN and NRPN | No | No | |
| System Exclusive | No | No | |
| MIDI Clock, Start, Continue and Stop | No | No | Plugin tempo sync uses tempo from the host |
| System Common and Active Sensing | No | No | |
| MPE | No | No | |

CC10 Pan and CC11 Expression are available through the two assignable MIDI
routes. Assign them in the MIDI tab to use them with a sound.

## Program Change in a host

Assign numbers from a favorite sound's context menu in the Library. The
**Program Change...** menu on the MIDI tab lists and removes assignments.
Instances using the same library share these assignments.

In VST3, the host passes the number through the plugin's program parameter.
Synth1X handles that number the same way as the MIDI message received in CLAP,
AUv2, or standalone. A recall can wait for the library, and the latest number
takes effect if several requests are pending.
[Program Change setup](gui/patch-library.md#program-change-slots)
