// Paleta de cores dos componentes — fonte única usada tanto no preview 3D
// (via legend_color(), chamado pelos outros parts/*.scad) quanto na
// legenda de imagem gerada por scripts/gerar_legenda.py. Se mudar uma cor
// ou nome aqui, atualize também a lista espelhada em scripts/gerar_legenda.py.
function legend_entries() = [
    ["chapa",      "BurlyWood",      "Chapa de acrílico (base, réplica do 7MS10)"],
    ["roda",       "Black",          "Rodas"],
    ["motor",      "DimGray",        "Suporte do motor"],
    ["kl25z",      "RoyalBlue",      "Placa FRDM-KL25Z"],
    ["driver",     "Crimson",        "Ponte H (driver) — único componente embaixo da chapa"],
    ["hw201",      "Purple",         "Sensor IR HW-201 (par, um por roda)"],
    ["hcsr04",     "SeaGreen",       "Sensor ultrassônico HC-SR04"],
    ["caster",     "Silver",         "Rodízio"],
    ["bateria9v",  "Orange",         "Baterias 9V"],
    ["powerbank",  "SaddleBrown",    "Powerbank 5V"],
    ["pcb",        "LimeGreen",      "PCB própria (10x10cm)"],
    ["pcb_pe",     "LightSlateGray", "Pés impressos da PCB"],
];

function legend_color(name) =
    let (match = [for (e = legend_entries()) if (e[0] == name) e[1]])
    len(match) > 0 ? match[0] : "Gray";
