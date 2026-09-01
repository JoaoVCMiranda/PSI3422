include <params.scad>
use <legend.scad>

// O furo do rodízio (Ø19.5mm, x=-91.3 y=0) já existe na peça real
// (importado junto com a silhueta em base_plate.scad) — aqui só o
// preview 3D do rodízio, pendurado abaixo da chapa.
module caster_preview() {
    color(legend_color("caster"))
        translate([caster_position[0], caster_position[1], -caster_height / 2])
            cylinder(d = caster_diameter + 6, h = caster_height, center = true);
}
