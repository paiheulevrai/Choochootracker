"""ChooChoo-authored DX7 sound-design recipes, CC0-1.0.
Deliberate independent voices; no ROM data, random mutations or count padding.
Operator order OP6..OP1. Descriptions/categories record design intent; listening
acceptance remains separate from numerical rendering checks.
"""
import json
from pathlib import Path
from dx7 import default_voice,validate
SHAPES={
 'held':([99,80,50,75],[99,92,88,0]),
 'pluck':([99,67,38,68],[99,72,30,0]),
 'wood':([99,78,53,81],[99,38,0,0]),
 'metal':([99,43,27,43],[99,88,67,0]),
 'pad':([37,28,21,38],[99,93,85,0]),
 'brass':([62,58,43,60],[99,81,77,0]),
 'tick':([99,93,77,90],[99,22,0,0]),
 'fade':([25,20,16,32],[99,90,80,0]),
}
def op(level,ratio=1,shape='held',**extra):
 r,l=SHAPES[shape];v=r+l+[48,0,0,0,0,2,0,2,level,0,1,0,7]
 if ratio==.5:v[18]=0
 else:v[18]=int(ratio);v[19]=round((ratio/int(ratio)-1)*100)
 for key,val in extra.items():v[{'fine':19,'dt':20,'vel':15,'ams':14,'mode':17,'bp':8,'rd':10,'rs':13,'rc':12}[key]]=val
 return v
Z=op(0)
def pairs(*ops):return list(ops)+[Z]*(6-len(ops))
# Each voice defines a separate musical role, envelope, routing and spectrum.
RECIPES=[
 ('Felt Hammer','Keys',4,0,pairs(op(66,2,'wood'),op(93,1,'pluck'),op(48,5,'tick'),op(79,1,'pluck',dt=9)),{}),
 ('Wire Harp','Keys',0,2,pairs(Z,Z,op(53,7,'tick'),op(70,2,'pluck'),op(77,3,'wood'),op(96,1,'metal')),{}),
 ('Muted Clav','Keys',4,5,pairs(op(83,2,'wood',rd=35),op(93,1,'wood'),op(63,5,'tick'),op(81,2,'wood')),{}),
 ('Celadon EP','Electric Piano',4,0,pairs(op(81,11,'tick',vel=7),op(91,1,'metal'),op(69,1,'pluck'),op(88,1,'metal',dt=9),op(54,3,'wood'),op(77,1,'pluck',dt=5)),{}),
 ('Reed EP','Electric Piano',0,3,pairs(Z,Z,op(49,4,'wood'),op(76,2,'pluck'),op(70,1,'pluck',vel=5),op(98,1,'metal')),{}),
 ('Moss Bass','Bass',0,5,pairs(Z,Z,Z,Z,op(69,1,'wood',rd=40),op(98,.5,'pluck')),{}),
 ('Pick Sub','Bass',4,1,pairs(op(70,3,'tick'),op(99,.5,'pluck'),op(53,2,'wood'),op(76,1,'pluck')),{}),
 ('Rubber Bass','Bass',0,7,pairs(Z,Z,Z,op(70,2,'tick'),op(89,1,'wood'),op(95,1,'held')),{126:90,127:58,130:62,131:50}),
 ('Hollow Fifth','Bass',31,0,pairs(op(94,.5,'held'),op(72,1.5,'pluck'),op(68,2,'wood')),{}),
 ('Copper Lead','Lead',0,6,pairs(Z,Z,op(42,7,'held'),op(67,3,'held'),op(77,1,'brass'),op(96,1,'held')),{139:9,143:3,138:25}),
 ('Whistle Glide','Lead',4,0,pairs(op(35,2,'brass'),op(96,1,'held')),{126:55,127:60,130:43,131:50,139:13,143:3,138:30}),
 ('Nasal Solo','Lead',0,5,pairs(Z,Z,Z,op(49,5,'brass'),op(77,2,'held'),op(95,1,'brass')),{139:7,143:3}),
 ('Cracked Square','Lead',4,7,pairs(op(91,1,'held',rd=35),op(84,1,'held'),op(68,3,'wood'),op(78,2,'held',dt=9)),{}),
 ('Cloud Loom','Pad',4,1,pairs(op(44,3,'fade'),op(91,1,'pad',dt=5),op(52,2,'pad'),op(81,1,'fade',dt=9),op(39,5,'fade'),op(73,2,'pad')),{}),
 ('Glass Horizon','Pad',4,0,pairs(op(60,3.5,'pad'),op(87,1,'fade'),op(43,7,'fade'),op(82,1,'pad',dt=10),op(49,5,'pad'),op(77,2,'fade',dt=4)),{139:6,143:2,138:40}),
 ('Breathing Choir','Pad',1,2,pairs(op(32,7,'pad'),op(41,5,'fade'),op(53,3,'pad'),op(88,1,'pad',ams=2),op(58,2,'fade'),op(91,1,'pad',ams=1,dt=9)),{140:34,137:18,142:4}),
 ('Night Current','Pad',21,3,pairs(op(45,1,'fade'),op(80,1,'pad'),op(74,2,'pad'),op(61,3,'fade'),op(54,4,'pad'),op(85,1,'fade',dt=5)),{139:12,143:3,137:12}),
 ('Bronze Cup','Bell-Mallet',4,0,pairs(op(84,3.42,'metal'),op(91,1,'metal'),op(73,7.28,'pluck'),op(79,2,'metal')),{}),
 ('Rosewood Bars','Bell-Mallet',4,1,pairs(op(79,4,'tick'),op(97,1,'pluck'),op(57,7,'wood'),op(73,1,'pluck',dt=9)),{}),
 ('Ice Pendulum','Bell-Mallet',4,2,pairs(op(62,9,'metal'),op(88,1,'metal',ams=3),op(71,5.4,'pluck'),op(78,2,'metal',ams=2)),{140:45,137:39,142:4}),
 ('Water Drops','Bell-Mallet',0,0,pairs(Z,Z,Z,Z,op(59,2,'wood'),op(98,1,'pluck')),{126:88,127:45,130:64,131:50}),
 ('Chapel Pipes','Organ',31,0,pairs(op(89,.5,vel=0),op(83,1,vel=0),op(74,2,vel=0),op(64,3,vel=0),op(61,4,vel=0),op(54,6,vel=0)),{}),
 ('Combo Reeds','Organ',21,4,pairs(op(63,2),op(87,1,vel=0),op(72,2,vel=0),op(63,4,vel=0),op(59,3),op(82,1,vel=0)),{}),
 ('Breath Flute','Brass-Wind',0,2,pairs(Z,Z,Z,Z,op(43,2,'brass'),op(99,1,'brass')),{139:12,143:3,138:46,137:38}),
 ('Low Horns','Brass-Wind',21,5,pairs(op(66,1,'brass'),op(84,1,'brass'),op(73,.5,'brass'),op(65,2,'brass'),op(71,1,'brass'),op(89,1,'brass',dt=9)),{}),
 ('Oboe Lantern','Brass-Wind',0,4,pairs(Z,Z,Z,op(41,5,'brass'),op(73,2,'brass'),op(96,1,'brass')),{139:8,143:3}),
 ('FM Low Tom','Percussion',4,0,pairs(op(70,2,'tick'),op(98,1,'wood')),{126:93,127:69,130:76,131:50,144:12}),
 ('Tin Snare','Percussion',0,7,pairs(op(87,19,'tick'),op(90,13,'wood'),op(95,7,'tick'),op(83,3,'wood'),op(80,1,'tick'),op(93,1,'wood')),{126:97,127:81,130:64,131:50}),
 ('Copper Shaker','Percussion',4,7,pairs(op(99,23,'tick',mode=1,fine=84),op(93,3,'tick',mode=1,fine=79),op(89,19,'wood',mode=1,fine=67),op(75,2,'tick',mode=1,fine=95)),{}),
 ('Relay Chatter','FX',0,7,pairs(op(94,13,'held'),op(88,7,'held'),op(86,5,'held'),op(80,3,'held'),op(71,2,'held'),op(89,1,'held')),{139:90,143:7,137:83,142:5}),
 ('Falling Metal','FX',4,3,pairs(op(91,7.7,'metal'),op(92,1,'metal'),op(81,11,'pluck'),op(82,2,'metal')),{126:42,127:25,128:18,130:78,131:51,132:28}),
 ('Beacon Pair','FX',4,0,pairs(op(43,2,'held',mode=1,fine=46),op(95,2,'held',mode=1,fine=75),op(48,3,'held',mode=1,fine=13),op(84,3,'held',mode=1,fine=21)),{140:80,137:46,142:3}),
]
def generate():
 voices=[]
 for name,category,algorithm,feedback,ops,globals_ in RECIPES:
  v=default_voice(name);v[:126]=sum(ops,[]);v[134]=algorithm;v[135]=feedback;v[136]=1;v[141]=1
  for key,value in globals_.items():v[key]=value
  voices.append(dict(name=name,category=category,voice=validate(v)))
 return dict(schema=1,author='ChooChooTracker contributors',license='CC0-1.0',voices=voices)
if __name__=='__main__':
 Path(__file__).with_name('sources').joinpath('dx7','choochoo-originals.json').write_text(json.dumps(generate(),indent=2)+'\n')
