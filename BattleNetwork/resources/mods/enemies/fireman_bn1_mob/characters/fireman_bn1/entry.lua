nonce = function() end 

function LoadTexture(path)
    return Engine.ResourceHandle.new().Textures:LoadFile(path)
end

function StreamMusic(path, loop)
    return Engine.ResourceHandle.new().Audio:Stream(path, loop)
end

local texture = nil
local fireRingTexture = nil
local waitTime = 0 -- timer used in MoveState
local anim = nil
local aiStateIndex = 1 
local aiState = {} -- setup in the battle_init function
local firstFrame = false
local attack = "FIREARM"
local bombx = {}
local bomby = {}

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
		anim:SetState("IDLE")
		anim:SetPlayback(Playback.Loop)
		firstFrame = false
	end
    if waitTime <= 0 then
        NextState()
    end
end

function MoveState(self, dt) 
	if firstFrame then
		anim:SetState("MOVE")
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
			--fire arm
			attack = "FIREARM"
			anim:SetState("FIREARM_PREP")
		elseif self:CurrentTile():X() == 5 then
			--fire tower
			attack = "FIRETOWER"
			anim:SetState("FIRETOWER_INIT")
		else
			--fire bomb
			attack = "FIREBOMB"
			anim:SetState("FIREBOMB_INIT")
		end
		anim:SetPlayback(Playback.Loop)
		firstFrame = false
	else
		if attack == "FIREARM" then
			if waitTime == 205 then
				anim:SetState("FIREARM_HEATUP")
				anim:SetPlayback(Playback.Loop)
			end
			if waitTime == 193 then
				anim:SetState("FIREARM")
				anim:SetPlayback(Playback.Loop)
				--first fire spawn lasts 190 frames
				self:Field():Spawn(Firearm_Fire(self), self:CurrentTile():X() - 1, self:CurrentTile():Y())
			end
			if waitTime == 190 then
				--second fire spawn
				self:Field():Spawn(Firearm_Fire(self), self:CurrentTile():X() - 2, self:CurrentTile():Y())
			end
			if waitTime == 187 then
				--third fire spawn
				self:Field():Spawn(Firearm_Fire(self), self:CurrentTile():X() - 3, self:CurrentTile():Y())
			end
		elseif attack == "FIRETOWER" then
			if waitTime == 251 then
				anim:SetState("FIRETOWER_PREP")
				anim:SetPlayback(Playback.Loop)
			end
			if waitTime == 205 then
				anim:SetState("FIRETOWER")
				anim:SetPlayback(Playback.Loop)
				self:Field():Spawn(Firetower_Fire(self), self:CurrentTile():X() - 1, self:CurrentTile():Y())
				waitTime = waitTime - 100
			end
		elseif attack == "FIREBOMB" then
			if waitTime == 257 then
				anim:SetState("FIREBOMB_PREP_1")
				anim:SetPlayback(Playback.Loop)
			end
			if waitTime == 160 then
				anim:SetState("FIREBOMB_PREP_2")
				anim:SetPlayback(Playback.Loop)
			end
			if waitTime == 145 then
				anim:SetState("FIREBOMB")
				anim:SetPlayback(Playback.Loop)
			end
			if waitTime == 135 then
				for x = 0, 2 do
					local taken = true
					while taken == true do
						bombx[x] = math.random(1, 3)
						bomby[x] = math.random(1, 3)
						--check if any previous bombs share a planned location
						taken = false
						for y = 0, x-1 do
							while ((bombx[x] == bombx[y]) and (bomby[x] == bomby[y])) do
								taken = true
							end
						end
					end
				end
				print(bombx[0])
				self:Field():Spawn(Firebomb(self, bombx[0], bomby[0]), self:CurrentTile():X(), self:CurrentTile():Y())
			end
			if waitTime == 125 then
				self:Field():Spawn(Firebomb(self, bombx[1], bomby[1]), self:CurrentTile():X(), self:CurrentTile():Y())
			end
			if waitTime == 115 then
				self:Field():Spawn(Firebomb(self, bombx[2], bomby[2]), self:CurrentTile():X(), self:CurrentTile():Y())
			end
			if waitTime == 105 then
				anim:SetState("FIREBOMB_END")
				anim:SetPlayback(Playback.Loop)
			end
			if waitTime == 100 then
				waitTime = 0
			end
		end
	end
	if waitTime <= 0 then
		NextState()
	end
end

function Firearm_Fire(fireman) 
	local fire = Battle.Spell.new(Team.Blue)
	fire:SetTexture(texture, true)
	fire:HighlightTile(Highlight.Solid)
	fire:SetLayer(-1)
	
	fire:SetHitProps(MakeHitProps(
        20, 
        Hit.Impact | Hit.Flash | Hit.Flinch, 
        Element.Fire, 
        fireman:GetID(), 
        Drag(Direction.None, 0)
        )
    )
	
	local fireAnim = fire:GetAnimation()
	local expiration = 190

    fireAnim:CopyFrom(anim)
    fireAnim:SetState("FIREARM_FIRE_INIT")
    fireAnim:SetPlayback(Playback.Loop)

    fire.updateFunc = function(self, dt) 
		if expiration == 187 then
			fireAnim:SetState("FIREARM_FIRE")
			fireAnim:SetPlayback(Playback.Loop)
		end
        if expiration < 189 then
			local hitbox = Battle.Hitbox.new(self:Team())
			hitbox:SetHitProps(fire:GetHitProps())
			local ymod = 0
			
			fire:Field():Spawn(hitbox, fire:CurrentTile():X(), fire:CurrentTile():Y() + ymod)
		end
		expiration = expiration - 1
		if expiration <= 0 then
			self:Delete()
		end
    end
	
	return fire
end

function Firetower_Fire(fireman) 
	local fire = Battle.Spell.new(Team.Blue)
	fire:SetTexture(texture, true)
	fire:HighlightTile(Highlight.Solid)
	
	local fireAnim = fire:GetAnimation()
	local expiration = 139
	
	fire:SetHitProps(MakeHitProps(
		15, 
		Hit.Impact | Hit.Flash | Hit.Flinch, 
		Element.Fire, 
		fireman:GetID(), 
		Drag(Direction.None, 0)
		)
	)

    fireAnim:CopyFrom(anim)
    fireAnim:SetState("FIRETOWER_FIRE_INIT")
    fireAnim:SetPlayback(Playback.Loop)

    fire.updateFunc = function(self, dt) 
		--3 frames no damage, on 17 spawn next, 137 no more hitbox, 139 despawn
		if expiration == 123 then
			fireAnim:SetState("FIRETOWER_FIRE")
			fireAnim:SetPlayback(Playback.Loop)
		end
		if expiration == 4 then
			fireAnim:SetState("FIRETOWER_FIRE_END")
			fireAnim:SetPlayback(Playback.Loop)
		end
        if expiration < 137 and expiration > 2 then
			fire:CurrentTile():AttackEntities(self)
		end
		if expiration == 122 and fire:CurrentTile():X() > 1 then
			local ymod = 0
			--local player = self:Target()
			--if player then
				--if player:CurrentTile():Y() > fire:CurrentTile():Y() then
					--ymod = ymod + 1
				--end
				--if player:CurrentTile():Y() < fire:CurrentTile():Y() then
					--ymod = ymod - 1
				--end
			--end
			self:Field():Spawn(Firetower_Fire(self), fire:CurrentTile():X() - 1, fire:CurrentTile():Y() + ymod)
		end
		expiration = expiration - 1
		if expiration <= 0 then
			self:Delete()
		end
    end
	
	fire.attackFunc = function(self, other)
		self:Delete()
	end
	
	return fire
end

function Firebomb(fireman, spawnx, spawny) 
	--75 frames to reach ground, [6 frames normal, 2 orange, 2 glowing, 2 orange]x5, 6 normal, [6 normal, 2 glowing, 2 white, 2 glowing]x3, [6 normal, 2 orange, 2 glowing, 10 white]
	local bomb = Battle.Spell.new(Team.Blue)
	bomb:SetTexture(texture, true)
	bomb:ShowShadow(true)
	
	local bombAnim = bomb:GetAnimation()
	local expiration = 197
	
	local dest = fireman:Field():TileAt(spawnx, spawny)

    bombAnim:CopyFrom(anim)
    bombAnim:SetState("FIREBOMB_BOMB_1")
    bombAnim:SetPlayback(Playback.Loop)

    bomb.updateFunc = function(self, dt) 
		if expiration == 122 then
			self:Field():Spawn(Firebomb_obstacle(self), self:CurrentTile():X(), self:CurrentTile():Y())
			--continue as obstacle
			self:Delete()
		end
		expiration = expiration - 1
    end
	
	bomb.canMoveToFunc = function(tile)
        return true
    end
	
	bomb:Jump(dest, 300.0, frames(75), frames(75), ActionOrder.Voluntary, nonce)
	
	return bomb
end

function Firebomb_obstacle(firebomb)
	--75 frames to reach ground, [6 frames normal, 2 orange, 2 glowing, 2 orange]x5, 6 normal, [6 normal, 2 glowing, 2 white, 2 glowing]x3, [6 normal, 2 orange, 2 glowing, 10 white]
	local bomb = Battle.Obstacle.new(Team.Blue)
	bomb:SetTexture(texture, true)
	bomb:ShowShadow(true)
	bomb:SetHealth(10)
	bomb:ShareTile(true)
	
	local bombAnim = bomb:GetAnimation()
	local expiration = 122

    bombAnim:CopyFrom(anim)
    bombAnim:SetState("FIREBOMB_BOMB_2")
    bombAnim:SetPlayback(Playback.Loop)

    bomb.updateFunc = function(self, dt) 
		if expiration == 56 then
			bombAnim:SetState("FIREBOMB_BOMB_3")
			bombAnim:SetPlayback(Playback.Loop)
		end
		if expiration == 20 then
			bombAnim:SetState("FIREBOMB_BOMB_4")
			bombAnim:SetPlayback(Playback.Loop)
		end
		expiration = expiration - 1
		if expiration <= 0 then
			self:Field():Spawn(Battle.Explosion.new(1, 1.0), bomb:CurrentTile():X(), bomb:CurrentTile():Y())
			self:Field():Spawn(Firebomb_Fire(self), bomb:CurrentTile():X(), bomb:CurrentTile():Y())
			self:Delete()
		end
    end
	return bomb
end

function Firebomb_Fire(firebomb)
	local fire = Battle.Spell.new(Team.Blue)
	fire:SetTexture(fireRingTexture, true)

    local ringanim = fire:GetAnimation()
	ringanim:Load(_modpath.."firering.animation")
    ringanim:SetState("IDLE")
    ringanim:SetPlayback(Playback.Loop)
	fire:HighlightTile(Highlight.Solid)
	
	fire:SetLayer(-2)
	
	fire:SetHitProps(MakeHitProps(
        10, 
        Hit.Impact | Hit.Flash | Hit.Flinch, 
        Element.Fire, 
        firebomb:GetID(), 
        Drag(Direction.None, 0)
        )
    )
	
	local expiration = 200

    fire.updateFunc = function(self, dt) 
		fire:CurrentTile():AttackEntities(self)
		expiration = expiration - 1
		if expiration <= 0 then
			self:Delete()
		end
    end
	
	fire.attackFunc = function(self, other)
		self:Delete()
	end
	
	return fire
end

function battle_init(self)
	math.randomseed(914191)
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

    texture = LoadTexture(_modpath.."FireMan.png")
	fireRingTexture = LoadTexture(_modpath.."firering.png")

    print("modpath: ".._modpath)
    self:SetName("FireManBN1")
    self:SetHealth(300)
    self:SetTexture(texture, true)
    self:SetHeight(60)
    self:ShareTile(false)

    anim = self:GetAnimation()
    anim:Load(_modpath.."fireman.animation")
    anim:SetState("IDLE")
    anim:SetPlayback(Playback.Loop)
end

function num_of_explosions()
	anim:SetState("FIREBOMB_PREP_1")
    return 11
end

function on_update(self, dt)
	--self:RegisterStatusCallback(Hit.Flinch, local function()
		--dayRuined()
	--end)
	waitTime = waitTime - 1
    aiState[aiStateIndex](self, dt)
end

function dayRuined()
	--reset AI pattern when flinched, stunned, etc
	aiStateIndex = 1
end

function can_move_to(tile) 
    if tile:IsEdge() then
        return false
    end

    return true
end