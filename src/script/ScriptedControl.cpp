
#include "script/ScriptedControl.h"

#include <sstream>

#include "core/Core.h"
#include "core/GameTime.h"
#include "core/Localisation.h"
#include "graphics/Math.h"
#include "graphics/GraphicsModes.h"
#include "gui/Speech.h"
#include "physics/Attractors.h"
#include "physics/Collisions.h"
#include "io/PakReader.h"
#include "io/FilePath.h"
#include "platform/String.h"
#include "scene/Interactive.h"
#include "scene/GameSound.h"
#include "script/ScriptUtils.h"

using std::string;

extern long GLOBAL_MAGIC_MODE;

namespace script {

namespace {

class ActivatePhysicsCommand : public Command {
	
public:
	
	ActivatePhysicsCommand() : Command("activatephysics", ANY_IO) { }
	
	Result execute(Context & context) {
		
		DebugScript("");
		
		ARX_INTERACTIVE_ActivatePhysics(GetInterNum(context.getIO()));
		
		return Success;
	}
	
};

class AttractorCommand : public Command {
	
public:
	
	AttractorCommand() : Command("attractor") { }
	
	Result execute(Context & context) {
		
		string target = context.getWord();
		
		INTERACTIVE_OBJ * t = inter.getById(target, context.getIO());
		
		string power = context.getWord();
		
		float val = 0.f;
		float radius = 0.f;
		
		if(power != "off") {
			val = context.getFloatVar(power);
			radius = context.getFloat();
		}
		
		DebugScript(' ' << target << ' ' << val << ' ' << radius);
		
		ARX_SPECIAL_ATTRACTORS_Add(GetInterNum(t), val, radius);
		
		return Success;
	}
	
};

class AmbianceCommand : public Command {
	
public:
	
	AmbianceCommand() : Command("ambiance") { }
	
	Result execute(Context & context) {
		
		HandleFlags("v") {
			if(flg & flag('v')) {
				float volume = context.getFloat();
				fs::path ambiance = fs::path::load(context.getWord());
				DebugScript(' ' << options << ' ' << volume << ' ' << ambiance);
				bool ret = ARX_SOUND_PlayScriptAmbiance(ambiance, ARX_SOUND_PLAY_LOOPED, volume * 0.01f);
				if(!ret) {
					ScriptWarning << "unable to find ambiance " << ambiance;
					return Failed;
				}
				return Success;
			}
			return Failed;
		}
		
		fs::path ambiance = fs::path::load(context.getWord());
		DebugScript(' ' << ambiance);
		if(ambiance == "kill") {
			ARX_SOUND_KillAmbiances();
		} else {
			bool ret = ARX_SOUND_PlayScriptAmbiance(ambiance);
			if(!ret) {
				ScriptWarning << "unable to find " << ambiance;
				return Failed;
			}
		}
		
		return Success;
	}
	
};

class AnchorBlockCommand : public Command {
	
public:
	
	AnchorBlockCommand() : Command("anchorblock", ANY_IO) { }
	
	Result execute(Context & context) {
		
		bool choice = context.getBool();
		
		DebugScript(' ' << choice);
		
		ANCHOR_BLOCK_By_IO(context.getIO(), choice ? 1 : 0);
		
		return Success;
	}
	
};

class AttachCommand : public Command {
	
public:
	
	AttachCommand() : Command("attach") { }
	
	Result execute(Context & context) {
		
		string sourceio = context.getWord();
		INTERACTIVE_OBJ * t = inter.getById(sourceio, context.getIO());
		
		string source = context.getWord(); // source action_point
		
		string targetio = context.getWord();
		INTERACTIVE_OBJ * t2 = inter.getById(targetio, context.getIO());
		
		string target = context.getWord();
		
		DebugScript(' ' << sourceio << ' ' << source << ' ' << targetio << ' ' << target);
		
		ARX_INTERACTIVE_Attach(GetInterNum(t), GetInterNum(t2), source, target);
		
		return Success;
	}
	
};

class CineCommand : public Command {
	
public:
	
	CineCommand() : Command("cine") { }
	
	Result execute(Context & context) {
		
		bool preload = false;
		HandleFlags("p") {
			if(flg & flag('p')) {
				preload = true;
			}
		}
		
		string name = context.getWord();
		
		DebugScript(' ' << options << " \"" << name << '"');
		
		if(name == "kill") {
			DANAE_KillCinematic();
		} else if(name == "play") {
			PLAY_LOADED_CINEMATIC = 1;
			ARX_TIME_Pause();
		} else {
			
			if(resources->getFile(fs::path("graph/interface/illustrations") / (name + ".cin"))) {
				WILL_LAUNCH_CINE = name + ".cin";
				CINE_PRELOAD = preload;
			} else {
				ScriptWarning << "unable to find cinematic \"" << name << '"';
				return Failed;
			}
		}
		
		return Success;
	}
	
};

class SetGroupCommand : public Command {
	
public:
	
	SetGroupCommand() : Command("setgroup", ANY_IO) { }
	
	Result execute(Context & context) {
		
		bool rem = false;
		HandleFlags("r") {
			rem = test_flag(flg, 'r');
		}
		
		string group = context.getStringVar(context.getWord());
		
		DebugScript(' ' << options << ' ' << group);
		
		INTERACTIVE_OBJ & io = *context.getIO();
		if(group == "door") {
			if(rem) {
				io.GameFlags &= ~GFLAG_DOOR;
			} else {
				io.GameFlags |= GFLAG_DOOR;
			}
		}
		
		if(group.empty()) {
			ScriptWarning << "missing group";
			return Failed;
		}
		
		if(rem) {
			io.groups.erase(group);
		} else {
			io.groups.insert(group);
		}
		
		return Success;
	}
	
};

class ZoneParamCommand : public Command {
	
public:
	
	ZoneParamCommand() : Command("zoneparam") { }
	
	Result execute(Context & context) {
		
		string options = context.getFlags();
		string command = context.getWord();
		
		if(command == "stack") {
			DebugScript(" stack");
			ARX_GLOBALMODS_Stack();
			
		} else if(command == "unstack") {
			DebugScript(" unstack");
			ARX_GLOBALMODS_UnStack();
			
		} else if(command == "rgb") {
			
			desired.depthcolor.r = context.getFloat();
			desired.depthcolor.g = context.getFloat();
			desired.depthcolor.b = context.getFloat();
			desired.flags |= GMOD_DCOLOR;
			
			DebugScript(" rgb " << desired.depthcolor.r << ' ' << desired.depthcolor.g << ' ' << desired.depthcolor.b);
			
		} else if(command == "zclip") {
				
			desired.zclip = context.getFloat();
			desired.flags |= GMOD_ZCLIP;
			
			DebugScript(" zclip " << desired.zclip);
			
		} else if(command == "ambiance") {
			
			fs::path ambiance = fs::path::load(context.getWord());
			
			DebugScript(" ambiance " << ambiance);
			
			bool ret = ARX_SOUND_PlayZoneAmbiance(ambiance);
			if(!ret) {
				ScriptWarning << "unable to find ambiance " << ambiance;
			}
			
		} else {
			ScriptWarning << "unknown command: " << command;
			return Failed;
		}
		
		return Success;
	}
	
};

class MagicCommand : public Command {
	
public:
	
	MagicCommand() : Command("magic") { }
	
	Result execute(Context & context) {
		
		GLOBAL_MAGIC_MODE = context.getBool() ? 1 : 0;
		
		DebugScript(' ' << GLOBAL_MAGIC_MODE);
		
		return Success;
	}
	
};

class DetachCommand : public Command {
	
public:
	
	DetachCommand() : Command("detach") { }
	
	Result execute(Context & context) {
		
		string source = context.getWord(); // source IO
		string target = context.getWord(); // target IO
		
		DebugScript(' ' << source << ' ' << target);
		
		INTERACTIVE_OBJ * t = inter.getById(source, context.getIO());
		if(!t) {
			ScriptWarning << "unknown source: " << source;
			return Failed;
		}
		
		INTERACTIVE_OBJ * t2 = inter.getById(target, context.getIO());
		if(!t2) {
			ScriptWarning << "unknown target: " << target;
			return Failed;
		}
		
		ARX_INTERACTIVE_Detach(GetInterNum(t), GetInterNum(t2));
		
		return Success;
	}
	
};

}

void setupScriptedControl() {
	
	ScriptEvent::registerCommand(new ActivatePhysicsCommand);
	ScriptEvent::registerCommand(new AttractorCommand);
	ScriptEvent::registerCommand(new AmbianceCommand);
	ScriptEvent::registerCommand(new AnchorBlockCommand);
	ScriptEvent::registerCommand(new AttachCommand);
	ScriptEvent::registerCommand(new CineCommand);
	ScriptEvent::registerCommand(new SetGroupCommand);
	ScriptEvent::registerCommand(new ZoneParamCommand);
	ScriptEvent::registerCommand(new MagicCommand);
	ScriptEvent::registerCommand(new DetachCommand);
	
}

} // namespace script