/*
 * ScriptEvent.h
 *
 *  Created on: Jan 31, 2011
 *      Author: bmonkey
 */

#ifndef ARX_SCRIPTING_SCRIPTEVENT_H
#define ARX_SCRIPTING_SCRIPTEVENT_H

#include <map>

#include "script/Script.h"

struct SCRIPT_EVENT {
	SCRIPT_EVENT(const std::string & str): name(str) {}
	std::string name;
};

class ScriptEvent;

namespace script {

//! strip [] brackets
std::string loadUnlocalized(const std::string & str);

class Command;

} // namespace script

class ScriptEvent {
	
public:
	
	static long totalCount;
	
	ScriptEvent();
	virtual ~ScriptEvent();
	
	static ScriptResult send(EERIE_SCRIPT * es, ScriptMessage msg, const std::string & params, INTERACTIVE_OBJ * io, const std::string & eventname, long info = 0);
	
	static void registerCommand(script::Command * command);
	
	static void init();
	
private:
	
	typedef std::map<std::string, script::Command *> Commands;
	static Commands commands;
	
};

#endif // ARX_SCRIPTING_SCRIPTEVENT_H