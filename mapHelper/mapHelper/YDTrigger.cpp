#include "stdafx.h"
#include "YDTrigger.h"
#include "TriggerEditor.h"
#include "WorldEditor.h"


YDTrigger::YDTrigger()
	:m_bEnable(true)
{
	
}

YDTrigger::~YDTrigger()
{

}

YDTrigger* YDTrigger::getInstance()
{
	static YDTrigger instance;

	return &instance;
}

bool YDTrigger::isEnable()
{
	return m_bEnable;
}

void YDTrigger::onGlobals(BinaryWriter& writer)
{
	writer.write_string("#include <YDTrigger/Import.h>\n");
	writer.write_string("#include <YDTrigger/YDTrigger.h>\n");
}

void YDTrigger::onEndGlobals(BinaryWriter& writer)
{
	writer.write_string("#include <YDTrigger/Globals.h>\n");
	writer.write_string("endglobals\n");
	writer.write_string("#include <YDTrigger/Function.h>\n");
}

bool YDTrigger::onRegisterEvent(std::string& events,Trigger* trigger,Action* action,std::string& name)
{
	if (name == "YDWEDisableRegister")
	{
		m_triggerHasDisable[trigger] = true;
		return false;
	}
		

	m_hasAnyPlayer = false;
	//�����¼��� �����Ӷ����� Ԥ����� �Ƿ��� ������� 
	std::function<bool(Parameter**,uint32_t)> seachAnyPlayer = [&](Parameter** params,uint32_t count)
	{
		for (int i = 0; i < count; i++)
		{
			Parameter* param = params[i];
			Parameter**  childs = NULL;
			uint32_t child_count = 0;
			switch (param->typeId)
			{
			case Parameter::Type::preset:
				if (!strcmp(param->value, "PlayerALL"))
					return true;
				break;
			case Parameter::Type::variable:
				if (param->arrayParam)
				{
					child_count = 1;
					childs = &param->arrayParam;
				}
				break;
			case Parameter::Type::function:
				child_count = param->funcParam->param_count;
				childs = param->funcParam->parameters;
				break;
			}
			if (child_count > 0 && childs)
			{
				return seachAnyPlayer(childs, child_count);
			}
		}
		return false;
	};

	if (seachAnyPlayer(action->parameters, action->param_count))
	{
		events += "#define YDTRIGGER_COMMON_LOOP(n) ";
		m_hasAnyPlayer = true;
	}
	
	return true;
}

void YDTrigger::onRegisterEvent2(std::string& events, Trigger* trigger, Action* action, std::string& name)
{
	if (m_hasAnyPlayer)
	{
		events += "#define YDTRIGGER_COMMON_LOOP_LIMITS (0, 15)\n";
		events += "#include <YDTrigger/Common/loop.h>\n";
		m_hasAnyPlayer = false;
	}
}

bool YDTrigger::onActionToJass(std::string& output, Action* action,Action* parent, std::string& pre_actions, const std::string& trigger_name, bool nested)
{
	if (!action->enable)
		return false;
	TriggerEditor* editor = TriggerEditor::getInstance();
	int& stack = editor->space_stack;
	
	Parameter** parameters = action->parameters;
	switch (hash_(action->name))
	{
	case "YDWEForLoopLocVarMultiple"s_hash:
	{
		std::string variable = std::string("ydul_") + action->parameters[0]->value;
		output += "set " + variable + " = ";
		output += editor->resolve_parameter(parameters[1], trigger_name, pre_actions, parameters[1]->type_name) + "\n";
		output += editor->spaces[stack];
		output += "loop\n";
		output += editor->spaces[++stack];
		output += "exitwhen " + variable + " > " + editor->resolve_parameter(parameters[2], trigger_name, pre_actions, parameters[2]->type_name) + "\n";
		for (uint32_t i = 0; i < action->child_count; i++)
		{
			Action* childAction = action->child_actions[i];
			output += editor->spaces[stack];
			output += editor->convert_action_to_jass(childAction, action, pre_actions, trigger_name, false) + "\n";
		}
		output += editor->spaces[stack];
		output += "set " + variable + " = " + variable + " + 1\n";
		output += editor->spaces[--stack];
		output += "endloop\n";
		return true;
	}
	case "YDWEEnumUnitsInRangeMultiple"s_hash:
	{
		if (m_isInYdweEnumUnit) break;
		m_isInYdweEnumUnit = true;

		output += "set ydl_group = CreateGroup()\n";
		output += editor->spaces[stack];
		output += "call GroupEnumUnitsInRange(ydl_group,";
		output += parameters[0]->value;
		output += ",";
		output += parameters[1]->value;
		output += ",";
		output += parameters[2]->value;
		output += ",null)\n";
		output += editor->spaces[stack];
		output += "loop\n";

		output += editor->spaces[++stack];
		output += "set ydl_unit = FirstOfGroup(ydl_group)\n";

		output += editor->spaces[stack];
		output += "exitwhen ydl_unit == null\n";

		output += editor->spaces[stack];
		output += "call GroupRemoveUnit(ydl_group, ydl_unit)\n";
		for (uint32_t i = 0; i < action->child_count; i++)
		{
			Action* childAction = action->child_actions[i];
			output += editor->spaces[stack];
			output += editor->convert_action_to_jass(childAction, action, pre_actions, trigger_name, false) + "\n";
		}
		output += editor->spaces[--stack];
		output += "endloop\n";
		output += editor->spaces[stack];
		output += "call DestroyGroup(ydl_group)\n";

		m_isInYdweEnumUnit = false;
		return true;
	}
	case "YDWESaveAnyTypeDataByUserData"s_hash:
	{
		output += "call YDUserDataSet(";
		output += parameters[0]->value + 11; //typename_01_integer  + 11 = integer
		output += ",";
		output += editor->resolve_parameter(parameters[1], trigger_name, pre_actions, parameters[1]->type_name);
		output += ",\"";
		output += parameters[2]->value;
		output += "\",";
		output += parameters[3]->value + 11;
		output += ",";
		output += editor->resolve_parameter(parameters[4], trigger_name, pre_actions, parameters[4]->type_name);
		output += ")\n";
		return true;
	}
	case "YDWEFlushAnyTypeDataByUserData"s_hash:
	{
		output += "call YDUserDataClear(";
		output += parameters[0]->value + 11; //typename_01_integer  + 11 = integer
		output += ",";
		output += editor->resolve_parameter(parameters[1], trigger_name, pre_actions, parameters[1]->type_name);
		output += ",\"";
		output += parameters[3]->value;
		output += "\",";
		output += parameters[2]->value + 11;
		output += ")\n";

		return true;
	}
	case "YDWEFlushAllByUserData"s_hash:
	{
		output += "call YDUserDataClearTable(";
		output += parameters[0]->value + 11; //typename_01_integer  + 11 = integer
		output += ",";
		output += editor->resolve_parameter(parameters[1], trigger_name, pre_actions, parameters[1]->type_name);
		output += ")\n";
		return true;
	}

	case "YDWEExecuteTriggerMultiple"s_hash:
	{
		output += "set ydl_trigger = ";
		output += editor->resolve_parameter(parameters[0], trigger_name, pre_actions, parameters[0]->type_name);
		output += "\n" + editor->spaces[stack];
		output += "YDLocalExecuteTrigger(ydl_trigger)\n";

		for (int i = 0; i < action->child_count; i++)
		{
			Action* childAction = action->child_actions[i];

			output += editor->spaces[stack];
			output += editor->convert_action_to_jass(childAction, action, pre_actions, trigger_name, false) + "\n";
		}
		output += editor->spaces[stack];
		output += "call YDTriggerExecuteTrigger(ydl_trigger,";
		output += editor->resolve_parameter(parameters[1], trigger_name, pre_actions, parameters[1]->type_name);
		output += ")\n";
		return true;
		
	}
	case "YDWETimerStartMultiple"s_hash:
	{
		output += "set ydl_timer = CreateTimer()\n";

		for (int i = 0; i < action->child_count; i++)
		{
			Action* childAction = action->child_actions[i];
			if (childAction->child_flag == 0)//����ǲ������Ķ���
			{

			}
			else
			{

			}
			
		}

	}

	case "YDWESetAnyTypeLocalVariable"s_hash:
	{
		//���ݵ�ǰ��������ֲ�������λ�� ���������ɵĴ���
		std::string callname;
		std::string handle;
		if (parent == NULL)//������ڴ�����
		{
			callname = "YDLocal1Set";
		}
		else
		{
			switch (hash_(parent->name))
			{
			//������������ʱ����
			case "YDWETimerStartMultiple"s_hash:
			{
				if (action->child_flag == 1) //1�ǲ�����
					handle = "ydl_timer";
				else //�����Ƕ�����
					handle = "GetExpiredTimer()";
				callname = "YDLocalSet";
				break;
			}
			//����������촥������
			case "YDWERegisterTriggerMultiple"s_hash:
			{
				if (action->child_flag == 1) //1�ǲ�����
					handle = "ydl_trigger";
				else //�����Ƕ�����
					handle = "GetTriggeringTrigger()";
				
				callname = "YDLocalSet";
				break;
			}
			//������� �������ж���������
			case "YDWEExecuteTriggerMultiple"s_hash:
			{
				callname = "YDLocal5Set"; 
				break;
			}
			//���� ������δ����Ķ�������
			default:
			{
				if (m_isInMainProc)
					callname = "YDLocal1Set";
				else
					callname = "YDLocal2Set";
			}
				
			}
		}
		std::string var_name = parameters[1]->value;
		std::string var_type= parameters[0]->value + 11;
		
		if (!setHashLocal(var_name, var_type))
		{
			auto it = m_HashLocalTable.find(var_name);
			output += "\n��ʹ���˾ֲ�������" + var_name + "��(����:" + var_type;
			output += ")�������������ط�ʹ�õ��� " + it->second + "����)��\n";
		}
		output += "call " + callname + "(";
		if (!handle.empty()) //��handle������
		{
			output += handle + ",";
		}
		output += var_type + ",\"" + var_name + "\",";
		output += editor->resolve_parameter(parameters[2], trigger_name, pre_actions, parameters[2]->type_name);
		output += ")";
	}

	
	//case "YDWEGetAnyTypeLocalVariable"s_hash:
	//{
	//	//���ݵ�ǰ��������ֲ�������λ�� ���������ɵĴ���
	//	std::string callname;
	//	std::string handle;
	//	if (parent == NULL)//������ڴ�����
	//	{
	//		callname = "YDLocal1Get";
	//	}
	//	else
	//	{
	//		switch (hash_(parent->name))
	//		{
	//			//������������ʱ����
	//		case "YDWETimerStartMultiple"s_hash:
	//		{
	//			if (action->child_flag == 1) //1�ǲ�����
	//				handle = "ydl_timer";
	//			else //�����Ƕ�����
	//				handle = "GetExpiredTimer()";
	//			callname = "YDLocalGet";
	//			break;
	//		}
	//		//����������촥������
	//		case "YDWERegisterTriggerMultiple"s_hash:
	//		{
	//			if (action->child_flag == 1) //1�ǲ�����
	//				handle = "ydl_trigger";
	//			else //�����Ƕ�����
	//				handle = "GetTriggeringTrigger()";
	//
	//			callname = "YDLocalGet";
	//			break;
	//		}
	//		//���� ������δ����Ķ�������
	//		default:
	//		{
	//			if (m_isInMainProc)
	//				callname = "YDLocal1Get";
	//			else
	//				callname = "YDLocal2Get";
	//		}
	//
	//		}
	//	}
	//}
	}

	return false;
}

bool YDTrigger::onParamterToJass(std::string& output, Parameter* paramter, std::string& pre_actions, const std::string& trigger_name, bool nested)
{
	TriggerEditor* editor = TriggerEditor::getInstance();

	switch (paramter->typeId)
	{
	case Parameter::Type::function:
	{
		Action* action = paramter->funcParam;
		if (!action) break;
		Parameter** parameters = action->parameters;
		switch (hash_(action->name))
		{
		case "YDWELoadAnyTypeDataByUserData"s_hash:
		{
			output += "YDUserDataGet(";
			output += parameters[0]->value + 11; //typename_01_integer  + 11 = integer
			output += ",";
			output += editor->resolve_parameter(parameters[1], trigger_name, pre_actions, parameters[1]->type_name);
			output += ",\"";
			output += parameters[2]->value;
			output += "\",";
			output += paramter->type_name;
			output += ")";
			return true;
		}
		case "YDWEHaveSavedAnyTypeDataByUserData"s_hash:
		{
			output += "YDUserDataHas(";
			output += parameters[0]->value + 11; //typename_01_integer  + 11 = integer
			output += ",";
			output += editor->resolve_parameter(parameters[1], trigger_name, pre_actions, parameters[1]->type_name);
			output += ",\"";
			output += parameters[3]->value;
			output += "\",";
			output += parameters[2]->value + 11;
			output += ")";
		}
		case "GetEnumUnit"s_hash:
		{
			if (m_isInYdweEnumUnit)
				output += "ydl_unit";
			else
				output += "GetEnumUnit()";
			return true;
		}
		case "GetFilterUnit"s_hash:
		{
			if (m_isInYdweEnumUnit)
				output += "ydl_unit";
			else
				output += "GetFilterUnit()";
			return true;
		}
		case "YDWEForLoopLocVarIndex"s_hash:
		{
			std::string varname = action->parameters[0]->value;
			output = "ydul_" + varname;
			return true;
		}
		}
	}

	}
	return false;
}

void YDTrigger::onActionsToFuncBegin(std::string& funcCode,Trigger* trigger, Action* parent)
{
	if (!trigger && !parent)
		return;
	bool isSeachHashLocal = parent == NULL;

	//�����������Ƿ������õ�����ֲ�����
	std::function<void(Parameter**, uint32_t)> seachParam = [&](Parameter** params, uint32_t count)
	{
		for (int i = 0; i < count; i++)
		{
			Parameter* param = params[i];
			
			Parameter**  childs = NULL;
			uint32_t child_count = 0;
			switch (param->typeId)
			{
			case Parameter::Type::variable:
				if (param->arrayParam)
				{
					child_count = 1;
					childs = &param->arrayParam;
				}
				break;
			case Parameter::Type::function:
			{
				Action* action = param->funcParam;
				switch (hash_(action->name))
				{
				case "YDWEGetAnyTypeLocalVariable"s_hash:
				case "YDWEGetAnyTypeLocalArray"s_hash:
				{
					m_isInMainProc = true;
					return; //�������˾��˳��ݹ�
				}
				}
				child_count = action->param_count;
				childs = param->funcParam->parameters;
				break;
			}
			}
			if (child_count > 0 && childs)
			{
				seachParam(childs, child_count);
			}
		}
	};

	//������Ҫע��ľֲ�����
	std::function<void(Action**, uint32_t,Action*,bool,bool)> seachLocal = [&](Action** actions, uint32_t count,Action* parent_action,bool isSeachChild,bool isTimer)
	{
		for (int i = 0; i < count; i++)
		{
			Action* action = actions[i];
			//����������Ӷ��� and action���Ӷ���������
			//if (!isSeachChild && action->child_flag == 0)
			//	continue;
			
			uint32_t hash = hash_(action->name);
			if (isSeachHashLocal)
			{
				if (!isTimer)
				{
					//������ڼ�ʱ���� ����������
					if (hash == "YDWETimerStartMultiple"s_hash)
						isTimer = true;
				}
		
				seachParam(action->parameters, action->param_count);
			}

#define next(b) seachLocal(action->child_actions, action->child_count,action,b,isTimer);
			switch (hash)
			{
			case "IfThenElseMultiple"s_hash:
			case "ForLoopAMultiple"s_hash:
			case "ForLoopBMultiple"s_hash:
			case "ForLoopVarMultiple"s_hash:
			case "YDWERegionMultiple"s_hash:
			{
				next(true);
				break;
			}
			case "YDWEForLoopLocVarMultiple"s_hash:
			{
				std::string varname = action->parameters[0]->value;
				addLocalVar("ydul_" + varname, "integer");
				next(true);
				break;
			}
			case "EnumDestructablesInRectAllMultiple"s_hash:
			case "EnumDestructablesInCircleBJMultiple"s_hash:
			case "EnumItemsInRectBJMultiple"s_hash:
			{
				next(true);
				break;
			}
			case "ForGroupMultiple"s_hash:
			case "YDWEEnumUnitsInRangeMultiple"s_hash:
			{
				addLocalVar("ydl_group", "group");
				addLocalVar("ydl_unit", "unit");
				next(false);
				break;
			}

			case "YDWETimerStartMultiple"s_hash:
			{
				addLocalVar("ydl_timer", "timer");
				next(false);
				break;
			}
			case "YDWEExecuteTriggerMultiple"s_hash:
			{
				addLocalVar("ydl_triggerstep", "integer");
				addLocalVar("ydl_trigger", "trigger");
				next(false);
				break;
			}
			case "YDWESetAnyTypeLocalVariable"s_hash:
			case "YDWESetAnyTypeLocalArray"s_hash:
			{
				//ֻ������ʱ���е������������������ֲ�����
				if (isSeachHashLocal && (!isTimer || (isTimer && action->child_flag == 0)))
				{
					m_isInMainProc = true;
				}
				break;
			}

			}
#undef next(b)
		}

	};

	if (trigger)
	{
		seachLocal(trigger->actions, trigger->line_count,NULL,true,false);
	}
	else
	{
		seachLocal(parent->child_actions, parent->child_count,parent,true,false);
	}

	for (auto& i : m_localTable)
	{
		funcCode += "\tlocal " + i.type + " " + i.name;
		if (i.value.empty())
		{
			funcCode += "\n";
		}
		else
		{
			funcCode += " = " + i.value + "\n";
		}
	}

	if (m_isInMainProc)
	{
		funcCode += "\tcall YDLocalInitialize()\n";
	}
	
}

void YDTrigger::onActionsToFuncEnd(std::string& funcCode, Trigger* trigger, Action* parent)
{

	if (m_isInMainProc)
	{
		funcCode += "\tcall YDLocal1Release()\n";
	}
	for (auto& i : m_localTable)
	{
		switch (hash_(i.type.c_str()))
		{
		case "unit"s_hash:
		case "group"s_hash:
		case "timer"s_hash:
		case "trigger"s_hash:
		case "force"s_hash:
		{
			funcCode += "\tset " + i.name + " = null\n";
			break;
		}
		}
	}

	m_localTable.clear();
	m_localMap.clear();
	m_isInMainProc = false;
}

bool YDTrigger::hasDisableRegister(Trigger* trigger)
{
	auto it = m_triggerHasDisable.find(trigger);
	return it != m_triggerHasDisable.end();
}

void YDTrigger::addLocalVar(std::string name, std::string type, std::string value)
{
	if (m_localMap.find(name) != m_localMap.end())
		return;

	m_localTable.push_back({ name,type,value });
	m_localMap[name] = true;
}

bool YDTrigger::setHashLocal(std::string name, std::string type)
{
	TriggerEditor* editor = TriggerEditor::getInstance();
	std::string base_type = editor->get_base_type(type);
	auto it = m_HashLocalTable.find(name);
	if (it == m_HashLocalTable.end() && !type.empty())
	{
		m_HashLocalTable[name] = base_type;
		return true;
	}
	return it->second.compare(base_type) == 0;
}