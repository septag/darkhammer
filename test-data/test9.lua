s = eng.Scene()
s:clear()
s:createPhysicsPlane()

--eng.setSunDir(1, -1, 0)
eng.setSunIntensity(0.2)
eng.setAmbientSky(128, 128, 128)
eng.setAmbientGround(230, 255, 230)

core.printcon("Adding object")

ground = s:createModel("ground", "plane.h3dm")

obj = s:createModel("soldier", "soldier.h3dm")
-- obj:loadAnimation("soldier.h3da")
obj:loadCharacterAnim("controller1.json")
--obj = s:createModel("soldier", "soldier.h3dm")
--obj:loadCharacterAnim("test-animator.json")
obj["animchar"]:debug()

light = s:createPointLight("test")
light:move(0.5, 1, 0.5)
light["light"]["color"] = core.Color(1, 0, 0, 1)

eng.addTimer(30, "update_param")


function update_param(id)
    input = eng.Input()
    anim = obj:getCharacterAnim()
    if input:keyPressed(eng.KEY_UP) then
        anim:setParam("Direction", anim:getParam("Direction") + 0.01)
    elseif input:keyPressed(eng.KEY_DOWN) then
        anim:setParam("Direction", anim:getParam("Direction") - 0.01)
    end
    
    anim:setParam("Walk", input:keyPressed(eng.KEY_SPACE))
end