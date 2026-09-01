include <params.scad>
use <legend.scad>

// Bocais completos (caixa com tampa, sem plug), deitados lado a lado ao
// longo de X, na área aberta atrás dos motores.
module battery9v_preview() {
    color(legend_color("bateria9v"))
        translate(battery9v_position)
            for (i = [0 : battery9v_count - 1]) {
                xc = i * battery9v_spacing - (battery9v_count - 1) * battery9v_spacing / 2;
                translate([xc, 0, plate_top_z + battery9v_holder_footprint[2] / 2])
                    cube(battery9v_holder_footprint, center = true);
            }
}
