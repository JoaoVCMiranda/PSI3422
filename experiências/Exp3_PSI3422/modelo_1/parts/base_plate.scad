include <params.scad>
use <kl25z_mount.scad>
use <driver_mount.scad>
use <hw201_mount.scad>
use <hcsr04_mount.scad>
use <battery_holder.scad>
use <pcb_mount.scad>

// Silhueta real do Chassi 2WD REF: 7MS10 — importada de
// assets/2wd_robot/chassis_plate.stl (peça isolada do projeto de
// referência baixado pelo usuário), já com TODOS os furos e recortes
// originais do kit (cruzes dos motores, furo do rodízio, fendas etc.).
// Ao contrário do STL da KL25Z (1.4MB), esta peça é leve (532KB, extraída
// já isolada do .blend de referência) — projection()+import() aqui roda
// em <0.1s mesmo em $preview, então usamos a geometria real sempre (sem
// placeholder).
module chassis_outline_2d() {
    projection(cut = false) import(chassis_ref_stl);
}

module base_plate_2d() {
    difference() {
        chassis_outline_2d();
        union() {
            kl25z_mount();
            driver_mount_cutout();
            hw201_mount(motor_x_position,  motor_y_offset);
            hw201_mount(motor_x_position, -motor_y_offset);
            hcsr04_mount();
            battery_holder_9v();
            if (pcb_on_upper_level) pcb_mount();
        }
    }
}

module base_plate() {
    linear_extrude(height = plate_thickness)
        base_plate_2d();
}
