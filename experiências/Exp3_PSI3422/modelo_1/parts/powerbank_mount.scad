include <params.scad>

// Sem furo/cutout na chapa base: o powerbank fica sobre o deck da PCB
// (andar superior), preso por velcro — não compete por espaço na chapa
// já congestionada, e é tolerante à falta de modelo/dimensão definidos.
module powerbank_cradle_3d() {
    color("DimGray", 0.4)
        translate([
            pcb_position[0] + powerbank_deck_offset[0],
            pcb_position[1] + powerbank_deck_offset[1],
            pcb_standoff_height + pcb_thickness + powerbank_footprint[2] / 2
        ])
            cube(powerbank_footprint, center = true);
}
