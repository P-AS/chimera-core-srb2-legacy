-- boss.lua - the gate's properties leg: SRB2 Lua (an add-on the leg's
-- autoexec.cfg loads) that puts a boss in Greenflower Zone Act 1, which has
-- none, and hits it, so the Game State's Boss fields are known: an Egg Mobile
-- spawned ahead of the player (256 units east and north, 64 up) at leveltime
-- 30, hit once by the player at 40 (one health off, flashing until it is back
-- in its spawn state: leveltime 162 here). 2.1's Lua has no P_SpawnMobjFromMobj: the offsets are added by
-- hand.
local boss
addHook("PostThinkFrame", function()
	for player in players.iterate do
		if leveltime == 30 then
			local mo = player.mo
			boss = P_SpawnMobj(mo.x + 256*FRACUNIT, mo.y + 256*FRACUNIT, mo.z + 64*FRACUNIT, MT_EGGMOBILE)
		end
		if leveltime == 40 and boss and boss.valid then
			P_DamageMobj(boss, player.mo, player.mo)
		end
	end
end)
