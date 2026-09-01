include <params.scad>
use <legend.scad>
use <util.scad>
use <l298n_mount.scad>
use <hw201_mount.scad>
use <hcsr04_mount.scad>
use <pcb_mount.scad>

// A chapa É o STL real (assets/2wd_robot/chassis_plate.stl), importado
// como sólido 3D — sem projection()/linear_extrude e sem rotação: usamos
// o referencial nativo do arquivo, exatamente como extraído do .blend.
// KL25Z e nRF24 NÃO furam a chapa (ficam empilhados acima da PCB shield,
// que já tem seus próprios 4 pés/furos via pcb_standoff_holes_3d()).
module base_plate() {
    color(legend_color("chapa"))
    difference() {
        import(chassis_ref_stl);
        union() {
            l298n_holes_3d();
            hw201_holes_3d(motor_l_center);
            hw201_holes_3d(motor_r_center);
            hcsr04_holes_3d();
            pcb_standoff_holes_3d();
        }
    }
}
