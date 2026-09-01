include <../parts/params.scad>
use <../parts/battery_holder.scad>

translate([-battery9v_position[0], -battery9v_position[1], 0])
    battery9v_cradle_3d();
