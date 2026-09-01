include <params.scad>
use <legend.scad>

// Baterias 9V deitadas lado a lado ao longo de Y, presas por cradle
// impresso (bolso de fricção) — só furos de amarração (zip-tie) na chapa,
// sem fixação por parafuso na própria bateria.
module battery_holder_9v() {
    translate(battery9v_position)
        for (i = [0 : battery9v_count - 1]) {
            yc = i * battery9v_spacing - (battery9v_count - 1) * battery9v_spacing / 2;
            translate([0, yc])
                for (sx = [-1, 1])
                    translate([sx * (battery9v_body_footprint[0] / 2 + 3), 0])
                        circle(d = 2.5);
        }
}

module battery9v_cradle_3d() {
    color(legend_color("bateria9v"))
        translate(battery9v_position)
            for (i = [0 : battery9v_count - 1]) {
                yc = i * battery9v_spacing - (battery9v_count - 1) * battery9v_spacing / 2;
                translate([0, yc, battery9v_body_footprint[2] / 2])
                    cube(battery9v_body_footprint, center = true);
            }
}
