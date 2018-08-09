#pragma once
#include "ProtoParse.h"
#include "ProtobuffMessages.h"
#include <ctime>

struct wskin
{
	int wId;
	int paintkit;
};

std::vector<wskin> g_skins = {
	{ 508,622 },
	{ 5027,10007 },
	{ 1,527 },
	{ 2,544 },
	{ 3,387 },
	{ 4,586 },
	{ 32,338 },
	{ 36,404 },
	{ 61,653 },
	{ 63,270 },
	{ 30,652 },
	{ 7,656 },
	{ 8,305 },
	{ 10,260 },
	{ 13,629 },
	{ 14,401 },
	{ 16,632 },
	{ 60,383 },
	{ 17,433 },
	{ 19,283 },
	{ 24,688 },
	{ 25,616 },
	{ 26,306 },
	{ 27,198 },
	{ 28,483 },
	{ 29,434 },
	{ 33,481 },
	{ 34,262 },
	{ 35,450 },
	{ 39,311 },
	{ 38,232 },
	{ 11,628 },
	{ 9,344 },
	{ 40,538 }
};

#define START_MUSICKIT_INDEX 1500000
#define START_ITEM_INDEX     2000000

static void fix_null_inventory(ProtoWriter& cache);
static void clear_equip_state(ProtoWriter& object);
static void apply_medals(ProtoWriter& object);
static void apply_music_kits(ProtoWriter& object);
static void add_all_items(ProtoWriter& object);
static void add_item(ProtoWriter& object, int index, ItemDefinitionIndex itemIndex, int rarity, int paintKit, int seed, float wear, std::string name);
static TeamID GetAvailableClassID(int definition_index);
static int GetSlotID(int definition_index);
static std::vector<uint32_t> music_kits = { 3, 4, 5, 6, 7, 8 };


template<typename T>
inline std::string get_4bytes(T value)
{
	return std::string{ reinterpret_cast<const char*>( reinterpret_cast<void*>(&value) ), 4 };
}

template<typename T>
inline std::string make_econ_item_attribute(int def_index, T value)
{
	CSOEconItemAttribute attribute;
	attribute.add_def_index(def_index);
	attribute.add_value_bytes(get_4bytes(value));
	return attribute.serialize();
}

inline std::string make_equipped_state(int team, int slot)
{
	CSOEconItemEquipped equipped_state;
	equipped_state.add_new_class(team);
	equipped_state.add_new_slot(slot);
	return equipped_state.serialize();
}

static std::string inventory_changer(void *pubDest, uint32_t *pcubMsgSize) {
	CMsgClientWelcome msg((void*)((DWORD)pubDest + 8), *pcubMsgSize - 8);
	if (msg.getAll_outofdate_subscribed_caches().empty())
		return msg.serialize();

	CMsgSOCacheSubscribed cache(msg.get_outofdate_subscribed_caches().String());
	// If not have items in inventory, Create null inventory
	fix_null_inventory(cache); 
	// Add custom items
	auto objects = cache.getAll_objects();
	for (size_t i = 0; i < objects.size(); i++)
	{
		SubscribedType object(objects[i].String());

		if (!object.has_type_id())
			continue;

		switch (object.get_type_id().Int32())
		{
			case 1: // Inventory
			{
				if (true) //g_Options.skins_packet_clear_default_items
					object.clear_object_data();

				clear_equip_state(object);
				apply_medals(object);
				apply_music_kits(object);
				add_all_items(object);
				cache.replace_objects(object.serialize(), i);
			}
			break;
		}
	}
	msg.replace_outofdate_subscribed_caches(cache.serialize(), 0);
	
	return msg.serialize();
}

static bool inventory_changer_presend(void* pubData, uint32_t &cubData)
{
	CMsgAdjustItemEquippedState msg((void*)((DWORD)pubData + 8), cubData - 8);
	// Change music kit check
	if (msg.has_item_id()
		&& msg.get_new_class().UInt32() == 0
		|| msg.get_new_slot().UInt32() == 54)
	{
		auto ItemIndex = msg.get_item_id().UInt64() - START_MUSICKIT_INDEX;

		if (ItemIndex > 38 || ItemIndex < 3)
			return true;

		/*g_Options.skins_packets_musci_kit*/auto skins_packets_musci_kit = msg.get_new_slot().UInt32() == 0xFFFF ? 0 : ItemIndex - 2;

		return false;
	}
	// Change weapon skin check
	if (!msg.has_item_id()
		|| !msg.has_new_class()
		|| !msg.has_new_slot())
		return true;

	return false;
}

static void fix_null_inventory(ProtoWriter& cache)
{
	bool inventory_exist = false;
	auto objects = cache.getAll(CMsgSOCacheSubscribed::objects);
	for (size_t i = 0; i < objects.size(); i++)
	{
		ProtoWriter object(objects[i].String(), SubscribedType::MAX_FIELD);
		if (!object.has(SubscribedType::type_id))
			continue;
		if (object.get(SubscribedType::type_id).Int32() != 1)
			continue;
		inventory_exist = true;
		break;
	}
	if (!inventory_exist)
	{
		int cache_size = objects.size();
		ProtoWriter null_object(SubscribedType::MAX_FIELD);
		null_object.add(SubscribedType::type_id, 1);

		cache.add(CMsgSOCacheSubscribed::objects, null_object.serialize());
	}
}

static void clear_equip_state(ProtoWriter& object)
{
	auto object_data = object.getAll(SubscribedType::object_data);
	for (size_t j = 0; j < object_data.size(); j++)
	{
		ProtoWriter item(object_data[j].String(), CSOEconItem::MAX_FIELD);

		if (item.getAll(CSOEconItem::equipped_state).empty())
			continue;

		// create NOT equiped state for item
		auto null_equipped_state = make_equipped_state(0, 0);
		
		// unequip all
		auto equipped_state = item.getAll(CSOEconItem::equipped_state);
		for (size_t k = 0; k < equipped_state.size(); k++)
			item.replace(CSOEconItem::equipped_state, null_equipped_state, k);

		object.replace(SubscribedType::object_data, item.serialize(), j);
	}
}

std::vector<uint32_t> packets_medals = { 1372, 958, 957, 956, 955 };
int packets_equipped_medal = 874;

static void apply_medals(ProtoWriter& object)
{
	uint32_t steamid = g_SteamUser->GetSteamID().GetAccountID();

	ProtoWriter medal(CSOEconItem::MAX_FIELD);
	medal.add(CSOEconItem::account_id, steamid);
	medal.add(CSOEconItem::origin, 9);
	medal.add(CSOEconItem::rarity, 6);
	medal.add(CSOEconItem::quantity, 1);
	medal.add(CSOEconItem::quality, 4);
	medal.add(CSOEconItem::level, 1);

	// Time acquired attribute
	medal.add(CSOEconItem::attribute, make_econ_item_attribute(222, (uint32_t)std::time(0)));

	int i = 10000;
	for (uint32_t MedalIndex : packets_medals)
	{
		medal.add(CSOEconItem::def_index, MedalIndex);
		medal.add(CSOEconItem::inventory, i);
		medal.add(CSOEconItem::id, i);
		object.add(SubscribedType::object_data, medal.serialize());
		i++;
	}

	if (packets_equipped_medal)
	{
		medal.add(CSOEconItem::def_index, packets_equipped_medal);
		medal.add(CSOEconItem::inventory, i);
		medal.add(CSOEconItem::id, i);
		medal.add(CSOEconItem::equipped_state, make_equipped_state(0, 55));
		object.add(SubscribedType::object_data, medal.serialize());
	}
}

static void apply_music_kits(ProtoWriter& object)
{
	uint32_t steamid = g_SteamUser->GetSteamID().GetAccountID();

	ProtoWriter music_kit(CSOEconItem::MAX_FIELD);
	music_kit.add(CSOEconItem::account_id, steamid);
	music_kit.add(CSOEconItem::origin, 9);
	music_kit.add(CSOEconItem::rarity, 6);
	music_kit.add(CSOEconItem::quantity, 1);
	music_kit.add(CSOEconItem::quality, 4);
	music_kit.add(CSOEconItem::level, 1);
	music_kit.add(CSOEconItem::flags, 0);
	music_kit.add(CSOEconItem::def_index, 1314);

	// Time acquired attribute
	music_kit.add(CSOEconItem::attribute, make_econ_item_attribute(75, (uint32_t)std::time(0)));

	int selected_musickit_gui = 16;
	for (int i = 3; i <= 38; ++i)
	{
		if (selected_musickit_gui != i)
		{
			music_kit.add(CSOEconItem::attribute, make_econ_item_attribute(166, i)); // Music kit id
			music_kit.add(CSOEconItem::inventory, (START_MUSICKIT_INDEX + i));
			music_kit.add(CSOEconItem::id, (START_MUSICKIT_INDEX + i));
			object.add(SubscribedType::object_data, music_kit.serialize());
		}
	}

	if (selected_musickit_gui)
	{
		music_kit.add(CSOEconItem::attribute, make_econ_item_attribute(166, selected_musickit_gui)); // Music kit id
		music_kit.add(CSOEconItem::inventory, (START_MUSICKIT_INDEX + selected_musickit_gui));
		music_kit.add(CSOEconItem::id, (START_MUSICKIT_INDEX + selected_musickit_gui));
		music_kit.add(CSOEconItem::equipped_state, make_equipped_state(0, 54));
		object.add(SubscribedType::object_data, music_kit.serialize());
	}
}

static void add_all_items(ProtoWriter& object)
{
	int l = 1;
	for (auto& x : g_skins) {
		add_item(object, l, (ItemDefinitionIndex)x.wId, 6, x.paintkit, 0, FLT_MIN, "");
		l++;
	}
}

static void add_item(ProtoWriter& object, int index, ItemDefinitionIndex itemIndex, int rarity, int paintKit, int seed, float wear, std::string name)
{
	uint32_t steamid = g_SteamUser->GetSteamID().GetAccountID();

	ProtoWriter item(CSOEconItem::MAX_FIELD);
	item.add(CSOEconItem::id, (START_ITEM_INDEX + itemIndex));
	item.add(CSOEconItem::account_id, steamid);
	item.add(CSOEconItem::def_index, itemIndex);
	item.add(CSOEconItem::inventory, (START_ITEM_INDEX + index));
	item.add(CSOEconItem::origin, 24);
	item.add(CSOEconItem::quantity, 1);
	item.add(CSOEconItem::level, 1);
	item.add(CSOEconItem::style, 0);
	item.add(CSOEconItem::flags, 0);
	item.add(CSOEconItem::in_use, true);
	item.add(CSOEconItem::original_id, 0);
	item.add(CSOEconItem::rarity, rarity);
	item.add(CSOEconItem::quality, 0);

	if (name.size() > 0)
		item.add(CSOEconItem::custom_name, name);

	// Add equipped state for both teams
	TeamID avalTeam = GetAvailableClassID(itemIndex);

	if (avalTeam == TeamID::TEAM_SPECTATOR || avalTeam == TeamID::TEAM_TERRORIST) {
		item.add(CSOEconItem::equipped_state, make_equipped_state(TEAM_TERRORIST, GetSlotID(itemIndex)));
	}
	if (avalTeam == TeamID::TEAM_SPECTATOR || avalTeam == TeamID::TEAM_COUNTER_TERRORIST) {
		item.add(CSOEconItem::equipped_state, make_equipped_state(TEAM_COUNTER_TERRORIST, GetSlotID(itemIndex)));
	}

	// Add CSOEconItemAttribute's
	item.add(CSOEconItem::attribute, make_econ_item_attribute(6, float(paintKit)));
	item.add(CSOEconItem::attribute, make_econ_item_attribute(7, float(seed)));
	item.add(CSOEconItem::attribute, make_econ_item_attribute(8, float(wear)));

	// Time acquired attribute
	item.add(CSOEconItem::attribute, make_econ_item_attribute(180, (uint32_t)std::time(0)));

	// Stickers
	for (int j = 0; j < 4; j++)
	{
		item.add(CSOEconItem::attribute, make_econ_item_attribute(113 + 4 * j, uint32_t(289 + j))); // Sticker Kit
		item.add(CSOEconItem::attribute, make_econ_item_attribute(114 + 4 * j, float(0.001f)));     // Sticker Wear
		item.add(CSOEconItem::attribute, make_econ_item_attribute(115 + 4 * j, float(1.f)));        // Sticker Scale
		item.add(CSOEconItem::attribute, make_econ_item_attribute(116 + 4 * j, float(0.f)));        // Sticker Rotation
	}

	object.add(SubscribedType::object_data, item.serialize());
}

static TeamID GetAvailableClassID(int definition_index)
{
	switch (definition_index)
	{
	case WEAPON_KNIFE_BAYONET:
	case WEAPON_KNIFE_FLIP:
	case WEAPON_KNIFE_GUT:
	case WEAPON_KNIFE_KARAMBIT:
	case WEAPON_KNIFE_M9_BAYONET:
	case WEAPON_KNIFE_TACTICAL:
	case WEAPON_KNIFE_FALCHION:
	case WEAPON_KNIFE_SURVIVAL_BOWIE:
	case WEAPON_KNIFE_BUTTERFLY:
	case WEAPON_KNIFE_PUSH:
	case WEAPON_ELITE:
	case WEAPON_P250:
	case WEAPON_CZ75A:
	case WEAPON_DEAGLE:
	case WEAPON_REVOLVER:
	case WEAPON_MP7:
	case WEAPON_UMP45:
	case WEAPON_P90:
	case WEAPON_BIZON:
	case WEAPON_SSG08:
	case WEAPON_AWP:
	case WEAPON_NOVA:
	case WEAPON_XM1014:
	case WEAPON_M249:
	case WEAPON_NEGEV:
	case GLOVE_STUDDED_BLOODHOUND:
	case GLOVE_SPORTY:
	case GLOVE_SLICK:
	case GLOVE_LEATHER_WRAP:
	case GLOVE_MOTORCYCLE:
	case GLOVE_SPECIALIST:
		return TEAM_SPECTATOR;

	case WEAPON_GLOCK:
	case WEAPON_AK47:
	case WEAPON_MAC10:
	case WEAPON_G3SG1:
	case WEAPON_TEC9:
	case WEAPON_GALILAR:
	case WEAPON_SG553:
	case WEAPON_SAWEDOFF:
		return TEAM_TERRORIST;

	case WEAPON_AUG:
	case WEAPON_FAMAS:
	case WEAPON_MAG7:
	case WEAPON_FIVESEVEN:
	case WEAPON_USP_SILENCER:
	case WEAPON_HKP2000:
	case WEAPON_MP9:
	case WEAPON_M4A1_SILENCER:
	case WEAPON_M4A1:
	case WEAPON_SCAR20:
		return TEAM_COUNTER_TERRORIST;

	default:
		return TEAM_UNASSIGNED;
	}
}

static int GetSlotID(int definition_index)
{
	switch (definition_index)
	{
	case WEAPON_KNIFE_BAYONET:
	case WEAPON_KNIFE_FLIP:
	case WEAPON_KNIFE_GUT:
	case WEAPON_KNIFE_KARAMBIT:
	case WEAPON_KNIFE_M9_BAYONET:
	case WEAPON_KNIFE_TACTICAL:
	case WEAPON_KNIFE_FALCHION:
	case WEAPON_KNIFE_SURVIVAL_BOWIE:
	case WEAPON_KNIFE_BUTTERFLY:
	case WEAPON_KNIFE_PUSH:
		return 0;
	case WEAPON_USP_SILENCER:
	case WEAPON_HKP2000:
	case WEAPON_GLOCK:
		return 2;
	case WEAPON_ELITE:
		return 3;
	case WEAPON_P250:
		return 4;
	case WEAPON_TEC9:
	case WEAPON_CZ75A:
	case WEAPON_FIVESEVEN:
		return 5;
	case WEAPON_DEAGLE:
	case WEAPON_REVOLVER:
		return 6;
	case WEAPON_MP9:
	case WEAPON_MAC10:
		return 8;
	case WEAPON_MP7:
		return 9;
	case WEAPON_UMP45:
		return 10;
	case WEAPON_P90:
		return 11;
	case WEAPON_BIZON:
		return 12;
	case WEAPON_FAMAS:
	case WEAPON_GALILAR:
		return 14;
	case WEAPON_M4A1_SILENCER:
	case WEAPON_M4A1:
	case WEAPON_AK47:
		return 15;
	case WEAPON_SSG08:
		return 16;
	case WEAPON_SG553:
	case WEAPON_AUG:
		return 17;
	case WEAPON_AWP:
		return 18;
	case WEAPON_G3SG1:
	case WEAPON_SCAR20:
		return 19;
	case WEAPON_NOVA:
		return 20;
	case WEAPON_XM1014:
		return 21;
	case WEAPON_SAWEDOFF:
	case WEAPON_MAG7:
		return 22;
	case WEAPON_M249:
		return 23;
	case WEAPON_NEGEV:
		return 24;
	case GLOVE_STUDDED_BLOODHOUND:
	case GLOVE_SPORTY:
	case GLOVE_SLICK:
	case GLOVE_LEATHER_WRAP:
	case GLOVE_MOTORCYCLE:
	case GLOVE_SPECIALIST:
		return 41;
	default:
		return -1;
	}
}