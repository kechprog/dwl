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
- Volume
- Modular file listeners
- Dynamic font size(dependant on bar hieght)
- All screens tags at one Component
- Horizontal alignment component
- Configurable components
- Change brightness on multiple screens 
- Touchscreen on/off Component
## Planned/Todo
- Cross-Screen moving of clients
- Cross-screen alt+tab
- Pen/Tablet on/off Component
- Tweakable tablet width/hieght
- Trackpad emulation via touchscreen(done partially)
