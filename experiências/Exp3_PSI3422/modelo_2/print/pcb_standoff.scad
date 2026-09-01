include <../parts/params.scad>

// Um único pé (os 4 são idênticos) — imprimir 4x.
cylinder(d = pcb_hole_inset * 1.6, h = pcb_standoff_height);
