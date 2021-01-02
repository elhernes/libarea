
const area = require('./build/Release/area.node')

var circle_prop = {
    tool_diameter: 0.250,
    mode: "SpiralPocketMode", // "ZigZagPocketMode" "SingleOffsetPocketMode" "ZigZagThenSingleOffsetPocketMode"
    zz_angle: 45,
    units_scale: 25.4,
    tolerance: 0.001
};

var circle_cut = {
    finish: false,
    xyCut: 50, // % of tool diameter
    position: [
	{     xc: 0., // x-center
              yc: 0., // y-center
	      rInner: 0.375, // inner island radius
	      rOuter: 1.250, // outer radius
	} ]
};

//var tp = area.circle_pocket(circle_prop, circle_cut);
//console.log(tp);

var poly_prop = {
    tool_diameter: 0.393700787402, // 10mm
    mode: "ZigZagThenSingleOffsetPocketMode",
    zz_angle: 0,
    units_scale: 25.4,
    tolerance: 0.001
};

var poly_cut_front_v = {
    xyCut: 45, // % of tool diameter
    points: [
	{ x: 2.896, y: -0.867 },
	{ x: 1.854, y: 0.175 },
	{ x: 0.813, y: -0.867 }
    ]
};

var poly_cut_rear_v = {
    xyCut: 45, // % of tool diameter
    points: [
	{ x: 1.854, y: -0.625 },
	{ x: 2.896, y: 0.417 },
	{ x: 0.813, y: 0.417 }
    ]
};

var tpa = area.polygon_pocket(poly_prop, poly_cut_rear_v);
console.log(tpa);

var nc_lines = [];

nc_lines.push("100 START INS 01");
nc_lines.push("FR XY =3.2000");
nc_lines.push("FR Z =1.0000");
nc_lines.push("TD =0.394");
nc_lines.push("SETUP>zcxyu");
nc_lines.push("SPINDLE ON");

function _moveTo(mob) {
    var cmd = "";
    if ((('abs' in mob)) || mob.abs) {
	cmd += "GO";
    } else {
	cmd += "GR";
    }
    cmd += "f ";
    var axis = ['x', 'y', 'z'];
    for(const a of axis) {
	if (mob[a] !== undefined) {
	    var ln = cmd + a.toUpperCase();
            ln += (mob[a] < 0) ? "-" : " ";
            ln += Math.abs(mob[a]).toFixed(4);
            nc_lines.push(ln);
	}
    }
}

function _cutTo(mob) {
    var indent = "     "; // 5 spaces
    var cmd = "";
    if ((('abs' in mob)) || mob.abs) {
	cmd += "GO ";
    } else {
	cmd += "GR ";
    }
    var axis = ['x', 'y', 'z'];
    for(const a of axis) {
	if (mob[a] !== undefined) {
	    cmd += a.toUpperCase();
	    cmd += (mob[a] < 0) ? "-" : " ";
	    cmd += Math.abs(mob[a]).toFixed(4);
	    nc_lines.push(cmd);
	    cmd = indent;
	}
    }
}

function tp_to_dynalang(tp, nPass, zDepth, zStart) {
    var zIncr = zDepth/(nPass);
    _moveTo({x:tp[0].x, y:tp[0].y, abs:true});
    _moveTo({z:zStart, abs:true});
    if (nPass>1) {
	nc_lines.push("REPEAT " + (("0"+nPass).slice(-2)));
    }
    _cutTo({z:-zIncr});
    for(const pt of tp) {
	_cutTo({x:pt.x, y:pt.y, abs:true});
    }
    _cutTo({x:tp[0].x, y:tp[0].y, abs:true});
    if (nPass>1) {
	nc_lines.push("REPEAT END");
    }
}

nc_lines.push("Z>C");
tp_to_dynalang(tpa[0], 3, 0.500, 0.0)
tp_to_dynalang(tpa[1], 1, 0.500, 0.0)

nc_lines.forEach(function(ln, ix, ar) {
    console.log(ln);
});

    
