include <BOSL2/std.scad>

ENC = [200, 120, 90]; // Dimensions of the enclosure
WALL = 3; // Wall thickness
CH = 1; // Chamfer size
LID_LIP = 9; // Lid wedge size
LID_CLEARANCE = 0.4; // Clearance for the lid to fit

$fn = 100;
epsilon = 0.001;

base();

LID_INPLACE = [WALL+LID_CLEARANCE/2, WALL+LID_CLEARANCE/2, ENC.z-WALL]; // Position for the lid to be in place
LID_BESIDE = [0, -ENC.y, 0]; // Position for the lid to be beside the enclosure

translate(LID_BESIDE)
    lid();

module base() {
    // Base
    difference() {
        BASE = [ENC.x, ENC.y, WALL];
        cuboid(BASE, anchor = BOTTOM+LEFT+FRONT, chamfer=CH, except=TOP);
        hex_holes(BASE, diameter=10);
        translate([WALL, WALL, -epsilon]) cuboid([36, 36, WALL+2*epsilon], anchor = BOTTOM+LEFT+FRONT);
    }
    // PIR mount
    difference() {
        translate([WALL, WALL, 0]) cuboid([36, 36, WALL], anchor = BOTTOM+LEFT+FRONT);
        translate([WALL+18, WALL+18, -2*epsilon]) cyl(d=23, h=WALL + 4*epsilon, anchor = BOTTOM, chamfer=-CH);
    }

    // Left wall
    cuboid([WALL, ENC.y, ENC.z], anchor = BOTTOM+LEFT+FRONT, chamfer=CH, except=RIGHT);

    // Right wall
    difference() {
        right(ENC.x - WALL) cuboid([WALL, ENC.y, ENC.z], anchor = BOTTOM+LEFT+FRONT, chamfer=CH, except=LEFT);
        // Barral socket cutout
        up(ENC.z - 25) right(ENC.x+epsilon) back(25) rotate([90, 0, 90]) cyl(d=12, h=WALL+2*epsilon, anchor=TOP);
    }

    // Front wall
    difference() {
        cuboid([ENC.x, WALL, ENC.z], anchor = BOTTOM+LEFT+FRONT, chamfer=CH, except=BACK);
        // 3.5mm socket cutout
        up(ENC.z / 2) right(ENC.x - 25) fwd(epsilon) rotate([90, 0, 0]) cyl(d=8, h=WALL+2*epsilon, anchor=TOP);
    }

    // Back wall
    back(ENC.y - WALL) difference() {
        SCREEN_X_OFFSET = 20;
        SCREEN_MOUNT_X = 97;
        SCREEN_MOUNT_Z = 55;
        SCREEN_MOUNT_H = 5;
        SCREEN_CUTOUT = [92, WALL+epsilon*2, 39.5];
        union() {
            cuboid([ENC.x, WALL, ENC.z], anchor = BOTTOM+LEFT+FRONT, chamfer=CH, except=FRONT);
            // Screen mount
            for (x = [SCREEN_X_OFFSET + SCREEN_CUTOUT.x/2 - SCREEN_MOUNT_X/2, SCREEN_X_OFFSET + SCREEN_CUTOUT.x/2 + SCREEN_MOUNT_X/2]) {
                for (z = [ENC.z/2 - SCREEN_MOUNT_Z/2, ENC.z/2 + SCREEN_MOUNT_Z/2]) {
                    translate([x, 0, z]) rotate([90, 0, 0]) 
                        cyl(d=8, h=SCREEN_MOUNT_H, anchor = BOTTOM);
                }
            }
        }
        // Screen mount
        for (x = [SCREEN_X_OFFSET + SCREEN_CUTOUT.x/2 - SCREEN_MOUNT_X/2, SCREEN_X_OFFSET + SCREEN_CUTOUT.x/2 + SCREEN_MOUNT_X/2]) {
            for (z = [ENC.z/2 - SCREEN_MOUNT_Z/2, ENC.z/2 + SCREEN_MOUNT_Z/2]) {
                translate([x, -SCREEN_MOUNT_H-epsilon*2, z]) rotate([90, 0, 0]) 
                    #m3_hole(h=SCREEN_MOUNT_H+WALL/2);
            }
        }
        up((ENC.z - SCREEN_CUTOUT.z)/2) right(SCREEN_X_OFFSET) fwd(epsilon)
            cuboid(SCREEN_CUTOUT, anchor = BOTTOM+LEFT+FRONT);
        SWITCH_DIAM = 20;
        up(ENC.z/4) back(WALL+epsilon) rotate([90, 0, 0]) {
            right(ENC.x - 30) cylinder(d=SWITCH_DIAM, h=WALL + 2*epsilon);
            right(ENC.x - 60) cylinder(d=SWITCH_DIAM, h=WALL + 2*epsilon);
        }
        // LED cuttout
        up(ENC.z/2) back(WALL+epsilon) right(ENC.x - 45) rotate([90, 0, 0])
            cylinder(d=5, h=WALL+2*epsilon);
    }

    // Lid wedges
    up(ENC.z - WALL) difference() {
        union() {
            // Front
            right(WALL) back(WALL) rotate([-90, 0, 0]) wedge([ENC.x-WALL*2, LID_LIP, LID_LIP]);
            // Left
            right(WALL) back(ENC.y-WALL) rotate([-90, 0, 270]) wedge([ENC.y-WALL*2, LID_LIP, LID_LIP]);
            // Right
            right(ENC.x-WALL) back(WALL) rotate([-90, 0, 90]) wedge([ENC.y-WALL*2, LID_LIP, LID_LIP]);
            // Back
            right(ENC.x-WALL) back(ENC.y-WALL) rotate([-90, 0, 180]) wedge([ENC.x-WALL*2, LID_LIP, LID_LIP]);
        }
        // Chamfer the edges
        LIP_CHAMFER = 2;
        translate([LID_LIP-LIP_CHAMFER+WALL, LID_LIP-LIP_CHAMFER+WALL, epsilon])
            cuboid([ENC.x-WALL*2-LID_LIP*2+LIP_CHAMFER*2, ENC.y-WALL*2-LID_LIP*2+LIP_CHAMFER*2, LID_LIP], anchor = TOP+LEFT+FRONT);
        // M3 screw holes
        for (x=[WALL + 4, ENC.x - WALL - 4]) {
            for (y=[WALL + 4, ENC.y - WALL - 4]) {
                translate([x, y, epsilon*2])
                    m3_hole(h = LID_LIP + 2*epsilon);
            }
        }
    }
}

module lid() {
    LID = [ENC.x - WALL*2 - LID_CLEARANCE, ENC.y - WALL*2 - LID_CLEARANCE, WALL];

    difference() {
        cuboid(LID, anchor = BOTTOM+LEFT+FRONT);
        hex_holes(LID);
        for (x=[4 - LID_CLEARANCE/2, ENC.x - 2*WALL - 4 - LID_CLEARANCE/2]) {
            for (y=[4 - LID_CLEARANCE/2, ENC.y - 2*WALL - 4 - LID_CLEARANCE/2]) {
                translate([x, y, WALL*1 + epsilon]) {
                    cyl(d=4, h=WALL+2*epsilon, anchor=TOP);
                    cyl(d1=4, d2=7, h=2, anchor=TOP);
                }
            }
        }
    }
}



module hex_holes(SIZE, diameter=10) {
    x_spacing = diameter * sqrt(3)/2 + 4; // Hexagonal grid spacing
    y_spacing = diameter; // Hexagonal grid spacing
    x_under = (SIZE.x - 2*diameter) % x_spacing; // Width grid falls short of the enclosure width
    y_under = (SIZE.y - 2*diameter) % y_spacing; // Height grid falls short of the enclosure height
    right(x_under/2) back(y_under/2) {
    for (x=[diameter : x_spacing : SIZE.x - diameter]) {
        for (y=[diameter : y_spacing*2 : SIZE.y - y_spacing*2]) {
            translate([x, y, -epsilon])
                regular_prism(n = 6, h = SIZE.z + 2*epsilon, r = diameter/2, center = false, realign=true);
        }
    }
    for (x=[diameter+x_spacing/2 : x_spacing : SIZE.x - diameter - x_spacing/2]) {
        for (y=[diameter+y_spacing : y_spacing*2 : SIZE.y - diameter]) {
            translate([x, y, -epsilon])
                regular_prism(n = 6, h = SIZE.z + 2*epsilon, r = diameter/2, center = false, realign=true);
        }
    }
    }
}

module m3_hole(h=1) {
    difference() {
        down(epsilon) cylinder(d=3, h=h, anchor=TOP);
        for (t=[0, 120, 240]) {
            rotate([0, 0, t]) {
                translate([3/2+0.5, 0, 0])
                    cylinder(d=3/6+1, h=h, anchor=TOP);
            }
        }
    }
}