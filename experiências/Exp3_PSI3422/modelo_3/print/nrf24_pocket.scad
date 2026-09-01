// Bolso do nRF24L01+ — imprimir 1x.
include <../parts/params.scad>

w = nrf24_pocket_wall;
difference() {
    cube([nrf24_footprint[0] + 2 * w, nrf24_footprint[1] + 2 * w, nrf24_footprint[2]], center = true);
    translate([0, 0, w])
        cube([nrf24_footprint[0], nrf24_footprint[1], nrf24_footprint[2]], center = true);
}
