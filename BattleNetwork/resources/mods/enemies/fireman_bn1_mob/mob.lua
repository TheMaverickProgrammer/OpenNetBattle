function load_scripts()
    Engine.DefineCharacter("Example.Fireman_bn1", _modpath.."characters/fireman_bn1")
end

function roster_init(info) 
    info:SetName("Fireman (BN1)")
    info:SetDescription("Go back to the start! The very first scenario boss in the series.")
    info:SetSpeed(1)
    info:SetAttack(15)
    info:SetHP(300)
    info:SetPreviewTexturePath(_modpath.."preview.png")
end

function build(mob) 
    print("_modpath is: ".._modpath)
    local texPath = _modpath.."background.png"
    local animPath = _modpath.."background.animation"
    mob:SetBackground(texPath, animPath, -1.4, 0.0)
    mob:StreamMusic(_modpath.."music.ogg")

    local fireman_bn1_spawner = mob:CreateSpawner("Example.Fireman_bn1")
    fireman_bn1_spawner:SpawnAt(5, 2)
end