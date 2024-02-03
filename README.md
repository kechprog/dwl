# Dwl with missing features
A fork of [dwl](https://codeberg.org/dwl/dwl), with much needed features implemented as well as heavily refactored and extended [bar for dwl](https://sr.ht/~raphi/somebar/). Original bar had a limitation of not being able to poll() for events and thus having any additional stuff(e.g. battery, time, etc.) required a lot of cpu time(compared to almost 0% in current implementation)
## Done
- Pen/Tablet support
- Touchscreen support
- Trackpad gestures
- Gaps
- Modular Components(Bar)
- Component Alignment
- Brightness
- Time
- Battery
- Modular file listeners
## Planned/Todo
- Volume Component
- Pen/Tablet on/off Component
- Touchscreen on/off Component
- All screens tags at one Component
- Dragging(it segfaults now)
- Tweakable tablet width/hieght
- Trackpad emulation via touchscreen(done partially)