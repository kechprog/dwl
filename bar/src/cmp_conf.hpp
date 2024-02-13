#include "BarComponent.hpp"

#define PADDING_X_DEF 6
#define PADDING_Y_DEF 2

namespace config::components {
	typedef TimeComponent<CmpStyle {
		.colors = {
		   /*|------text-------|---------bg----------|----------border----|*/
			{{36, 39, 58, 255}, {238, 153, 160, 255}, {255, 255, 255, 255}}, /* inactive */
			{{36, 39, 58, 255}, {237, 135, 150, 255}, {255, 255, 255, 255}}  /*  active  */
		},
		.align = 1,	
		.padding_x = PADDING_X_DEF,
		.padding_y = PADDING_Y_DEF
	}> Time;
	
	typedef BatteryComponent<CmpStyle {
		.colors = {
		   /*|------text-------|---------bg----------|----------border----|*/
			{{36, 39, 58, 255}, {238, 212, 159, 255}, {255, 255, 255, 255}}, /* inactive */
			{{36, 39, 58, 255}, {245, 169, 127, 255}, {255, 255, 255, 255}}  /*  active  */
		},
		.align = 1,	
		.padding_x = PADDING_X_DEF,
		.padding_y = PADDING_Y_DEF
	}> Battery;

	typedef VolComponent<CmpStyle {
		.colors = {
		   /*|------text-------|---------bg----------|----------border----|*/
			{{36, 39, 58, 255}, {139, 213, 202, 255}, {255, 255, 255, 255}}, /* inactive */
			{{36, 39, 58, 255}, {166, 218, 149, 255}, {255, 255, 255, 255}}  /*  active  */
		},
		.align = 1,	
		.padding_x = PADDING_X_DEF,
		.padding_y = PADDING_Y_DEF
	}> Volume;

	typedef BrightnessComponent<CmpStyle {
		.colors = {
		   /*|------text-------|---------bg----------|----------border----|*/
			{{36, 39, 58, 255}, {125, 196, 228, 255}, {255, 255, 255, 255}}, /* inactive */
			{{36, 39, 58, 255}, {138, 173, 244, 255}, {255, 255, 255, 255}}  /*  active  */
		},
		.align = 1,	
		.padding_x = PADDING_X_DEF,
		.padding_y = 0, 
	}>	Brightness;

	typedef TouchStateComponent<CmpStyle {
		.colors = {
		   /*|------text-------|---------bg----------|----------border----|*/
			{{36, 39, 58, 255}, {238, 212, 159, 255}, {255, 255, 255, 255}}, /* inactive */
			{{36, 39, 58, 255}, {245, 169, 127, 255}, {255, 255, 255, 255}}  /*  active  */
		},
		.align = 1,	
		.padding_x = PADDING_X_DEF,
		.padding_y = PADDING_Y_DEF
	}> TouchState;
	
	typedef LayoutComponent<CmpStyle {
		.colors = {
		   /*|------text-------|---------bg----------|----------border----|*/
			{{36, 39, 58, 255}, {183, 189, 248, 255}, {255, 255, 255, 255}}, /* inactive */
			{{36, 39, 58, 255}, {198, 160, 246, 255}, {255, 255, 255, 255}}  /*  active  */
		},
		.align = 0,	
		.padding_x = PADDING_X_DEF,
		.padding_y = PADDING_Y_DEF
	}, false> Layout;

	typedef TitleComponent<CmpStyle {
		.colors = {
		   /*|------text-------|---------bg----------|----------border----|*/
			{{36, 39, 58, 255}, {244, 219, 214, 255}, {255, 255, 255, 255}}, /* inactive */
			{{36, 39, 58, 255}, {240, 198, 198, 255}, {255, 255, 255, 255}}  /*  active  */
		},
		.align = 0,	
		.padding_x = PADDING_X_DEF,
		.padding_y = PADDING_Y_DEF
	}, false> Title;
};
