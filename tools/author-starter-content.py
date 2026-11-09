"""One-time, inspectable recipe authoring example; never runs during build/gameplay.
Writes only missing starter files. The emitted JSON/Lua are runtime authoring inputs.
"""
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
def write(path, data):
    target = ROOT / path
    target.parent.mkdir(parents=True, exist_ok=True)
    if not target.exists():
        target.write_text(json.dumps(data, indent=2) + '\n' if not isinstance(data, str) else data, encoding='utf-8')

steel=[.62,.68,.69]; dark=[.20,.25,.27]; wood=[.30,.17,.08]; gold=[.9,.69,.27]; skin=[.78,.55,.36]; cloth=[.82,.81,.67]
def part(kind,center,size,color,tint=0,motion=0,**kw):
    return dict(primitive=kind,center=center,size=size,color=color,tint=tint,motion=motion,**kw)
def box(c,s,color,tint=0,motion=0): return part('box',c,s,color,tint,motion)
def cone(c,s,color,tint=0,motion=0): return part('cone',c,s,color,tint,motion)
def ball(c,s,color,tint=0): return part('ellipsoid',c,s,color,tint)
def beam(a,b,r,color,tint=0,motion=0): return part('beam',a,[r,r,r],color,tint,motion,end=b)
def figure(kind):
    p=[]
    if kind=='siege':
        p += [box([0,.6,0],[2,.3,2.8],wood),box([0,.9,0],[1.6,.12,2.5],[.5,.32,.17])]
        for x in [-1.1,1.1]:
            for z in [-1,1]: p += [beam([x-.13,.42,z],[x+.13,.42,z],.46,dark)]
            p += [beam([x*.6,.8,-.9],[x*.6,2.3,0],.13,wood),beam([x*.6,.8,.9],[x*.6,2.3,0],.13,wood)]
        p += [beam([0,1.1,-1.5],[0,2.7,1.4],.16,[.57,.37,.18],motion=3),ball([0,2.7,1.4],[.42,.42,.42],[.5,.52,.49]),box([-.8,2,-.8],[.5,.7,.08],cloth,1)]
        return p
    robe=kind=='support'
    if robe: p += [cone([0,0,0],[.55,1.55,.3],cloth,.15),box([0,1.5,0],[.65,.55,.48],cloth,.15)]
    else:
        for x,m in [(-.22,1),(.22,2)]: p += [box([x,.35,0],[.27,.55,.43],dark,motion=m),box([x,.78,0],[.27,.48,.3],steel,motion=m)]
        p += [box([0,1.3,0],[.78,.8,.5],steel),box([0,1.28,.27],[.62,.62,.05],cloth,1),box([0,.96,0],[.8,.12,.52],wood)]
    p += [ball([0,2.02,0],[.32,.35,.3],skin if kind=='ranged' else steel),box([0,2.03,.29],[.37,.08,.035],dark)]
    p += [beam([-.45,1.6,0],[-.57,1.15,.3],.14,cloth if robe else steel),beam([.45,1.6,0],[.64,1.3,.3],.14,cloth if robe else steel,motion=3)]
    if kind=='ranged':
        p += [cone([0,2.2,0],[.4,.3,.04],[.24,.37,.19],.7),beam([.64,.8,.4],[.7,1.48,.74],.045,wood,motion=3),beam([.7,1.48,.74],[.64,2.2,.4],.045,wood,motion=3),beam([.64,.8,.4],[.64,2.2,.4],.012,cloth,motion=3),box([-.25,1.5,-.4],[.28,.9,.22],wood)]
    elif robe:
        p += [cone([0,2.25,0],[.36,.24,.05],cloth,.15),beam([.69,.12,.3],[.69,2.8,.3],.055,gold,motion=3),box([.69,2.55,.3],[.5,.085,.09],gold,motion=3),ball([.69,2.75,.3],[.14,.14,.14],[.92,1,.65])]
    else:
        p += [box([-.58,1.3,.47],[.62,.9,.1],cloth,1),box([-.58,1.3,.54],[.07,.68,.02],gold),box([-.58,1.3,.54],[.48,.07,.02],gold),box([.65,1.3,.3],[.085,.4,.085],wood,motion=3),box([.65,1.55,.3],[.42,.07,.09],gold,motion=3),box([.65,2.02,.3],[.12,.92,.05],steel,motion=3),cone([0,2.3,0],[.22,.28,.04],cloth,1)]
    return p

visuals=[dict(id=f'core:{name}',parts=figure(kind)) for name,kind in [('soldier','melee'),('archer','ranged'),('cleric','support'),('catapult','siege')]]
castle=[box([0,.2,0],[11,.4,11],[.33,.36,.33]),box([0,2.5,0],[4.8,5,5.2],[.58,.58,.51]),part('roof',[0,5,0],[5.4,2.1,5.8],[.25,.32,.35])]
for x in [-4.5,4.5]:
    for z in [-4.5,4.5]:
        castle += [cone([x,0,z],[1.3,5.5,1.3],[.55,.57,.51]),cone([x,5.4,z],[1.5,.4,1.5],[.73,.72,.62])]
        for j in range(4):
            import math
            a=j*math.pi/2
            castle += [box([x+1.13*math.cos(a),6.1,z+1.13*math.sin(a)],[.65,.8,.65],[.65,.65,.57])]
    castle += [box([x,1.6,0],[.7,3.2,7.5],[.55,.57,.51])]
castle += [box([0,1.6,-4.5],[8,3.2,.7],[.55,.57,.51])]
for x in [-3,3]: castle += [box([x,1.6,4.5],[3,3.2,.7],[.55,.57,.51])]
castle += [beam([0,5.5,0],[0,9,0],.06,wood),box([.8,8.3,0],[1.6,1.1,.06],cloth,1)]
tree=[cone([0,0,0],[.22,3.8,.11],wood),ball([0,4,0],[1.65,1.7,1.6],[.23,.4,.12]),ball([.9,3.4,.1],[1.05,1.1,1.1],[.32,.47,.16])]
house=[box([0,1.1,0],[3,2.2,3.5],[.78,.71,.54]),part('roof',[0,2.2,0],[3.5,1.5,4],[.49,.25,.13]),box([0,.75,1.77],[.7,1.5,.06],wood)]
for x in [-1,1]: house += [box([x,1.5,1.78],[.45,.6,.06],dark)]
bridge=[]
for x in [-3.1,3.1]:
    bridge += [box([x,.65,0],[.25,.2,6],[.62,.58,.47])]
    for z in [-2.8,0,2.8]: bridge += [box([x,.42,z],[.35,1.1,.35],[.62,.58,.47])]
horse=[ball([0,1,0],[.55,.55,1.1],[.36,.23,.14]),ball([0,1.7,1],[.27,.6,.35],[.36,.23,.14]),ball([0,2.05,1.25],[.3,.3,.5],[.36,.23,.14])]
for x in [-.36,.36]:
    for z in [-.7,.7]: horse += [box([x,.5,z],[.19,1,.23],wood)]
horse += [box([0,1.5,0],[.9,.18,.8],cloth,1)]
for name,parts in [('castle',castle),('tree',tree),('house',house),('bridge',bridge),('horse',horse),('projectile',[beam([0,0,-.5],[0,0,.7],.04,wood),cone([0,0,.65],[.1,.28,.01],steel)]),('effect',[ball([0,0,0],[.3,.3,.3],[1,.83,.36],1)])]: visuals.append(dict(id='core:'+name,parts=parts))

sounds=[]
for name,dur,f0,f1,noise,tone,gain,priority in [
    ('steel',.22,1200,450,.4,.5,.4,3),('bow',.17,430,140,.5,.4,.35,2),('siege',.7,95,28,.65,.8,.8,5),('impact',.35,180,45,.8,.4,.5,3),('heal',.68,520,1040,.04,.8,.36,3),('collapse',1.2,100,22,.8,.7,.9,8),('hoof',.14,120,45,.45,.6,.3,1),('magic',.45,180,650,.25,.7,.4,4)]:
    sounds.append(dict(id='core:'+name,duration=dur,frequency=f0,end_frequency=f1,noise=noise,tone=tone,gain=gain,priority=priority))
attacks=[]
for name,mode,damage,rng,cd,sound,extra in [
    ('sword','melee',18,2.8,1.05,'steel',{}),('arrow','projectile',14,24,1.65,'bow',dict(flight_speed=28,arc=.08)),('mend','heal',17,15,1.8,'heal',{}),('stone','projectile',62,38,5,'siege',dict(flight_speed=18,arc=.24,splash=5.5,building_multiplier=2.4))]:
    attacks.append(dict(id='core:'+name,mode=mode,damage=damage,range=rng,cooldown=cd,sound='core:'+sound,**extra))
units=[]
for name,title,atk,vis,hp,speed,caps,depth,weight in [
    ('swordsman','Swordsman','sword','soldier',115,3.6,['move','melee','guard'],1,5),('archer','Archer','arrow','archer',66,3.5,['move','projectile'],-7,3),('cleric','Cleric','mend','cleric',78,3.3,['move','heal'],-9,2),('catapult','Catapult','stone','catapult',150,2,['move','projectile','siege'],-15,1)]:
    units.append(dict(id='core:'+name,name=title,attack='core:'+atk,appearance='core:'+vis,health=hp,speed=speed,capabilities=caps,formation_depth=depth,weight=weight,radius=1.2 if name=='catapult' else .58))
war=[[1 if i==j else -1 for j in range(4)] for i in range(4)]
peace=[[1 if i==j else 0 for j in range(4)] for i in range(4)]
alliances=[[1 if i%2==j%2 else -1 for j in range(4)] for i in range(4)]
write('assets/core/pack.json',dict(schema=1,api=1,id='core',dependencies=[],replace=False,files=['definitions.json']))
write('assets/core/definitions.json',dict(schema=1,
    factions=[dict(id='core:'+slug,name=title,short=short,color=color) for slug,title,short,color in [('iron','The Iron Banner','Iron',[.27,.59,.96]),('ashen','The Ashen Court','Ashen',[.87,.24,.25]),('verdant','The Verdant Pact','Verdant',[.27,.73,.47]),('sunward','The Sunward League','Sunward',[.96,.71,.22])]],
    attacks=attacks,units=units,visuals=visuals,sounds=sounds,
    maps=[dict(id='core:green_march',name='The Green March',description='Open pasture, rolling hills and diverging approaches',radius=70,relief=3.4,rivers=0,grass=[.37,.51,.24],water=[.15,.43,.59]),dict(id='core:three_crossings',name='Three Crossings',description='Three waterways and nine raised bridge corridors',radius=70,relief=2.2,rivers=3,grass=[.32,.47,.24],water=[.13,.43,.61])],
    scenario=dict(starting_per_faction=16,reinforce_seconds=2.5,relations=war),
    presets=[dict(id='core:war',name='Every kingdom for itself',relations=war),dict(id='core:peace',name='Armistice',relations=peace),dict(id='core:alliances',name='Two alliances',relations=alliances)]))

warder=figure('melee')+[box([0,1.4,-.32],[.8,.95,.06],[.6,.48,.83],1),beam([-.75,1.2,.3],[-.75,2.7,.3],.055,gold)]
write('user/mods/wardens/pack.json',dict(schema=1,api=1,id='wardens',dependencies=['core'],replace=False,files=['definitions.json']))
write('user/mods/wardens/definitions.json',dict(schema=1,behaviors=[dict(id='wardens:guardian_policy',file='guardian.lua')],visuals=[dict(id='wardens:guardian_mesh',parts=warder)],equipment=[dict(id='wardens:travelling_kit',modifiers=[dict(value='movement.max_speed',additive=.2,multiplier=1.05)])],units=[
    dict(id='wardens:lantern_warden',name='Lantern Warden',attack='core:sword',appearance='wardens:guardian_mesh',behavior='wardens:guardian_policy',equipment='wardens:travelling_kit',health=135,speed=3.5,capabilities=['move','melee','guard'],weight=1,formation_depth=-2),
    dict(id='wardens:road_vanguard',name='Road Vanguard',attack='core:sword',appearance='wardens:guardian_mesh',behavior='wardens:guardian_policy',health=105,speed=4.2,capabilities=['move','melee','guard'],weight=.6,formation_depth=2,ignore_injured=True)]))
write('user/mods/wardens/guardian.lua','''-- Shared policy with an authored Vanguard exception, not a C++ identity branch.
return function(self)
    if self.ignore_injured then
        native.flank()
        return
    end
    local patient = nil
    local urgency = 0
    for _, actor in ipairs(native.observe(24)) do
        if actor.ally and actor.healer and actor.health < actor.max_health * 0.8 then
            local score = 1 - actor.health / actor.max_health - actor.distance / 100
            if score > urgency then
                urgency = score
                patient = actor.ref
            end
        end
    end
    if patient then native.guard(patient) else native.advance() end
end
''')
print('Starter content present; existing files preserved.')
