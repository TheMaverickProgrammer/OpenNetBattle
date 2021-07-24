nonce = function() end 

function LoadTexture(path)
    return Engine.ResourceHandle.new().Textures:LoadFile(path)
end

function StreamMusic(path, loop)
    return Engine.ResourceHandle.new().Audio:Stream(path, loop)
end

local texture = nil
local waitTime = 0 -- timer used in MoveState
local anim = nil
local aiStateIndex = 1 
local aiState = {} -- setup in the battle_init function
local firstFrame = false

function NextState()
    -- increment our AI state counter
    aiStateIndex = aiStateIndex + 1
    if aiStateIndex > #aiState then 
        aiStateIndex = 1
    end
end

function SetWaitTimer(seconds) 
    return function(self, dt) 
        waitTime = seconds
		firstFrame = true
        NextState()
    end 
end 

function WaitState(self, dt)
	if firstFrame then
		anim:SetState("PLAYER_IDLE")
		anim:SetPlayback(Playback.Loop)
		firstFrame = false
	end
    if waitTime <= 0 then
        NextState()
    end
end

function MoveState(self, dt) 
	if firstFrame then
		anim:SetState("PLAYER_MOVE")
		anim:SetPlayback(Playback.Once)
		firstFrame = false
	end
	--5 frames in, teleport to a random tile other than the current one
	if waitTime == 10 then 
		local randomx = self:CurrentTile():X()
		local randomy = self:CurrentTile():Y()
		while (randomx == self:CurrentTile():X() and randomy == self:CurrentTile():Y()) do
			randomx = math.random(4, 6)
			randomy = math.random(1, 3)
		end
		local dest = self:Field():TileAt(randomx, randomy)
		self:Teleport(dest, ActionOrder.Voluntary, nonce)
	end
	if waitTime <= 0 then
		NextState()
	end
end

function AttackState(self, dt) 
	if firstFrame then
		if self:CurrentTile():X() == 4 then
			anim:SetState("PLAYER_SHOOTING")
		elseif self:CurrentTile():X() == 5 then
			anim:SetState("PLAYER_SHOOTING")
		else
			anim:SetState("PLAYER_THROW")
		end
		anim:SetPlayback(Playback.Loop)
		firstFrame = false
	end
	if waitTime <= 0 then
		NextState()
	end
end

function battle_init(self)
	math.randomseed(1000)
	math.random(); math.random(); math.random()  --lua sucks
    aiStateIndex = 1
    aiState = { 
        SetWaitTimer(25),
		WaitState,
		SetWaitTimer(15),
        MoveState,
		SetWaitTimer(25),
		WaitState,
		SetWaitTimer(15),
        MoveState,
		SetWaitTimer(25),
		WaitState,
		SetWaitTimer(15),
        MoveState,
		SetWaitTimer(45),
		WaitState,
		SetWaitTimer(15),
        MoveState,
		SetWaitTimer(260),
		AttackState,
    }

    texture = LoadTexture(_modpath.."fireman_atlas.png")

    print("modpath: ".._modpath)
    self:SetName("FireMan BN1")
    self:SetHealth(300)
    self:SetTexture(texture, true)
    self:SetHeight(60)
    self:ShareTile(false)

    anim = self:GetAnimation()
    anim:Load(_modpath.."fireman_battle.animation")
    anim:SetState("PLAYER_IDLE")
    anim:SetPlayback(Playback.Loop)
end

function num_of_explosions()
    return 12
end

function on_update(self, dt)
	waitTime = waitTime - 1
    aiState[aiStateIndex](self, dt)
end

function can_move_to(tile) 
    if tile:IsEdge() then
        return false
    end

    return true
end