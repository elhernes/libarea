
const area = require('./build/Release/area.node')

var prop = {
    tool_diameter: 0.250,
    mode: "SpiralPocketMode", // "ZigZagPocketMode" "SingleOffsetPocketMode" "ZigZagThenSingleOffsetPocketMode"
    zz_angle: 45
};

var circle_cut = {
    finish: false,
    zCut: 50, // % of tool diameter
    xyCut: 50, // % of tool diameter
    zHeight: 0., // starting height
    zDepth: -0.500, // depth
    position: [
	{     xc: 0., // x-center
              yc: 0., // y-center
	      rInner: 0.375, // inner island radius
	      rOuter: 1.250, // outer radius
	} ]
};

var tp = area.circle_pocket(prop, circle_cut);
console.log(tp);

