/*
 *
 * (C) 2003-2010 Anope Team
 * Contact us at team@anope.org
 *
 * Please read COPYING and README for further details.
 *
 * Based on the original code of Epona by Lara.
 * Based on the original code of Services by Andy Church.
 */

/*************************************************************************/

#include "module.h"

class CommandNSSetMisc : public Command
{
 private:
	Anope::string Desc;

 protected:
	CommandReturn RealExecute(User *u, const std::vector<Anope::string> &params)
	{
		NickCore *nc = findcore(params[0]);
		if (!nc)
			throw CoreException("NULL nc in CommandNSSetMisc");

		nc->Shrink("nickserv:" + this->name);
		if (params.size() > 1)
		{
			nc->Extend("nickserv:" + this->name, new ExtensibleItemRegular<Anope::string>(params[1]));
			notice_lang(Config->s_NickServ, u, CHAN_SETTING_CHANGED, this->name.c_str(), nc->display.c_str(), params[1].c_str());
		}
		else
			notice_lang(Config->s_NickServ, u, CHAN_SETTING_UNSET, this->name.c_str(), nc->display.c_str());

		return MOD_CONT;
	}

 public:
	CommandNSSetMisc(const Anope::string &cname, const Anope::string &desc, const Anope::string &cpermission = "") : Command(cname, 0, 1, cpermission), Desc(desc)
	{
	}

	CommandReturn Execute(User *u, const std::vector<Anope::string> &params)
	{
		std::vector<Anope::string> realparams = params;
		realparams.insert(realparams.begin(), u->Account()->display);
		return RealExecute(u, realparams);
	}

	void OnSyntaxError(User *u, const Anope::string &)
	{
		syntax_error(Config->s_NickServ, u, "SET", NICK_SET_SYNTAX);
	}

	void OnServHelp(User *u)
	{
		u->SendMessage(Config->s_NickServ, "    %-10s    %s", this->name.c_str(), this->Desc.c_str());
	}
};

class CommandNSSASetMisc : public CommandNSSetMisc
{
 public:
	CommandNSSASetMisc(const Anope::string &cname, const Anope::string &desc) : CommandNSSetMisc(cname, desc, "nickserv/saset/" + cname)
	{
		this->MinParams = 1;
		this->MaxParams = 2;
	}

	CommandReturn Execute(User *u, const std::vector<Anope::string> &params)
	{
		return RealExecute(u, params);
	}

	void OnSyntaxError(User *u, const Anope::string &)
	{
		syntax_error(Config->s_NickServ, u, "SASET", NICK_SASET_SYNTAX);
	}
};

class NSSetMisc : public Module
{
	struct CommandInfo
	{
		Anope::string Name;
		Anope::string Desc;
		bool ShowHidden;

		CommandInfo(const Anope::string &name, const Anope::string &desc, bool showhidden) : Name(name), Desc(desc), ShowHidden(showhidden) { }
	};

	std::map<Anope::string, CommandInfo *> Commands;

	void RemoveAll()
	{
		if (Commands.empty())
			return;

		Command *set = FindCommand(NickServ, "SET");
		Command *saset = FindCommand(NickServ, "SASET");

		if (!set && !saset)
			return;

		for (std::map<Anope::string, CommandInfo *>::const_iterator it = this->Commands.begin(), it_end = this->Commands.end(); it != it_end; ++it)
		{
			if (set)
			{
				Command *c = set->FindSubcommand(it->second->Name);
				if (c)
				{
					set->DelSubcommand(c);
					delete c;
				}
			}
			if (saset)
			{
				Command *c = saset->FindSubcommand(it->second->Name);
				if (c)
				{
					saset->DelSubcommand(c);
					delete c;
				}
			}
		}

		this->Commands.clear();
	}

 public:
	NSSetMisc(const Anope::string &modname, const Anope::string &creator) : Module(modname, creator)
	{
		this->SetAuthor("Anope");
		this->SetType(CORE);

		Implementation i[] = { I_OnReload, I_OnNickInfo, I_OnDatabaseWriteMetadata, I_OnDatabaseReadMetadata };
		ModuleManager::Attach(i, this, 4);

		OnReload(true);
	}

	~NSSetMisc()
	{
		RemoveAll();
	}

	void OnReload(bool)
	{
		RemoveAll();

		Command *set = FindCommand(NickServ, "SET");
		Command *saset = FindCommand(NickServ, "SASET");
		if (!set && !saset)
			return;

		ConfigReader config;

		for (int i = 0, num = config.Enumerate("ns_set_misc"); i < num; ++i)
		{
			Anope::string cname = config.ReadValue("ns_set_misc", "name", "", i);
			if (cname.empty())
				continue;
			Anope::string desc = config.ReadValue("ns_set_misc", "desc", "", i);
			bool showhidden = config.ReadFlag("ns_set_misc", "operonly", "no", i);

			CommandInfo *info = new CommandInfo(cname, desc, showhidden);
			if (!this->Commands.insert(std::make_pair(cname, info)).second)
			{
				Alog() << "ns_set_misc: Warning, unable to add duplicate entry " << cname;
				delete info;
				continue;
			}

			if (set)
				set->AddSubcommand(new CommandNSSetMisc(cname, desc));
			if (saset)
				saset->AddSubcommand(new CommandNSSASetMisc(cname, desc));
		}
	}

	void OnNickInfo(User *u, NickAlias *na, bool ShowHidden)
	{
		for (std::map<Anope::string, CommandInfo *>::const_iterator it = this->Commands.begin(), it_end = this->Commands.end(); it != it_end; ++it)
		{
			if (!ShowHidden && it->second->ShowHidden)
				continue;

			Anope::string value;
			if (na->nc->GetExtRegular("nickserv:" + it->first, value))
				u->SendMessage(Config->s_NickServ, "      %s: %s", it->first.c_str(), value.c_str());
		}
	}

	void OnDatabaseWriteMetadata(void (*WriteMetadata)(const Anope::string &, const Anope::string &), NickCore *nc)
	{
		for (std::map<Anope::string, CommandInfo *>::const_iterator it = this->Commands.begin(), it_end = this->Commands.end(); it != it_end; ++it)
		{
			Anope::string value;
			if (nc->GetExtRegular("nickserv:" + it->first, value))
				WriteMetadata(it->first, ":" + value);
		}
	}

	EventReturn OnDatabaseReadMetadata(NickCore *nc, const Anope::string &key, const std::vector<Anope::string> &params)
	{
		for (std::map<Anope::string, CommandInfo *>::const_iterator it = this->Commands.begin(), it_end = this->Commands.end(); it != it_end; ++it)
			if (key == it->first)
				nc->Extend("nickserv:" + it->first, new ExtensibleItemRegular<Anope::string>(params[0]));

		return EVENT_CONTINUE;
	}
};

MODULE_INIT(NSSetMisc)