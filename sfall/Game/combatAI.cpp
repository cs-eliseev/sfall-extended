﻿/*
 *    sfall
 *    Copyright (C) 2008 - 2021 Timeslip and sfall team
 *
 */

#include "..\FalloutEngine\Fallout2.h"
#include "..\Modules\HookScripts\CombatHs.h"
#include "..\main.h"

#include "items.h"

#include "ImprovedAI\AI.Inventory.h"

#include "combatAI.h"

namespace game
{

namespace sf = sfall;

const long aiUseItemAPCost = 2;

// Implementation of ai_can_use_weapon_ engine function with the HOOK_CANUSEWEAPON hook
bool CombatAI::ai_can_use_weapon(fo::GameObject* source, fo::GameObject* weapon, long hitMode) {
	bool result = fo::func::ai_can_use_weapon(source, weapon, hitMode);
	return sf::CanUseWeaponHook_Invoke(result, source, weapon, hitMode);
}

static long drugUsePerfFixMode;

/*
	Describe the difference from the original function
*/
void __stdcall CombatAI::ai_check_drugs(fo::GameObject* source) {
	if (fo::func::critter_body_type(source)) return; // Robotic/Quadruped cannot use drugs

	DWORD slot = -1;
	long noInvenDrug = 0;
	bool drugWasUse = false;

	fo::GameObject* lastItem = nullptr; // combatAIInfoGetLastItem_(source); unused function, always returns 0
	if (!lastItem) {
		fo::AIcap* cap = fo::func::ai_cap(source);
		if (!cap) return;

		long hpPercent = 50;
		long chance = 0;

		switch (cap->chem_use)
		{
			case fo::AIpref::ChemUse::stims_when_hurt_little: // use only healing drugs
				hpPercent = 60;
				break;
			case fo::AIpref::ChemUse::stims_when_hurt_lots:   // use only healing drugs
				hpPercent = 30;
				break;
			case fo::AIpref::ChemUse::sometimes:
				if (!(fo::var::combatNumTurns % 3)) chance = 25; // every three turns
				//hpPercent = 50;
				break;
			case fo::AIpref::ChemUse::anytime:
				if (!(fo::var::combatNumTurns % 3)) chance = 75; // every three turns
				//hpPercent = 50;
				break;
			case fo::AIpref::ChemUse::always:
				chance = 100; // 99%
				break;
			case fo::AIpref::ChemUse::clean:
				return; // exit: don't use drugs
		}

		long minHP = (hpPercent * fo::func::stat_level(source, fo::Stat::STAT_max_hit_points)) / 100;

		// [FIX] Fixed not checking minimum hp properly for using healing drugs (prevents premature fleeing)
		if (cap->min_hp > minHP) minHP = cap->min_hp;

		while (fo::func::stat_level(source, fo::Stat::STAT_current_hp) < minHP && source->critter.getAP() >= aiUseItemAPCost)
		{
			fo::GameObject* itemFind = fo::func::inven_find_type(source, fo::ItemType::item_type_drug, &slot);
			if (!itemFind) {
				noInvenDrug = 2; // healing drugs were not found in the inventory (was 1)
				break;
			}

			if (game::Items::IsHealingItem(itemFind) && !game::Items::item_remove_mult(source, itemFind, 1, 0x4286F0)) { // [HOOK_REMOVEINVENOBJ]
				if (!game::Items::UseDrugItemFunc(source, itemFind)) {                                                   // [HOOK_USEOBJON]
					drugWasUse = true;
				}

				source->critter.decreaseAP(aiUseItemAPCost);
				slot = -1;
			}
		}

		// use any drug (exception healing drugs) if there is a chance of using it
		if (!drugWasUse && chance > 0 && fo::func::roll_random(0, 100) < chance) {
			long usedCount = 0;
			slot = -1; // [FIX] start the search from the first slot
			while (source->critter.getAP() >= aiUseItemAPCost)
			{
				fo::GameObject* item = fo::func::inven_find_type(source, fo::ItemType::item_type_drug, &slot);
				if (!item) {
					noInvenDrug = 1;
					break;
				}

				long counter = 0;

				if (drugUsePerfFixMode > 0) {
					fo::GameObject* firstFindDrug = item;
					long _slot = slot; // keep current slot
					do {
						// [FIX] Allow using only the drugs listed in chem_primary_desire and healing drugs (AIDrugUsePerfFix == 2)
						while (item->protoId != cap->chem_primary_desire[counter]) if (++counter > 2) break;
						if (counter <= 2) break; // there is a match

						// [FIX] Fixes for the chem_primary_desire preference list when using drugs
						if (drugUsePerfFixMode == 1) {
							counter = 0;
							item = fo::func::inven_find_type(source, fo::ItemType::item_type_drug, &slot);
							if (!item) {
								item = firstFindDrug;
								slot = _slot; // back to slot
								break;
							}
						}
					} while (counter < 3);
				} else {
					// if the drug is equal to the item in the preference list, then check the next [vanilla bug behavior]
					while (item->protoId == cap->chem_primary_desire[counter]) if (++counter > 2) break;
				}

				// if the preference counter is less than 3, then can use item drug
				if (counter < 3) {
					// if the item is NOT a healing drug
					if (!game::Items::IsHealingItem(item) && !game::Items::item_remove_mult(source, item, 1, 0x4286F0)) { // [HOOK_REMOVEINVENOBJ]
						if (!game::Items::UseDrugItemFunc(source, item)) {                                                // [HOOK_USEOBJON]
							drugWasUse = true;
							usedCount++;
						}

						source->critter.decreaseAP(aiUseItemAPCost);
						slot = -1;

						fo::AIpref::ChemUse chemUse = cap->chem_use;
						if (chemUse == fo::AIpref::ChemUse::sometimes ||
						   (chemUse == fo::AIpref::ChemUse::anytime && usedCount >= 2))
						{
							break;
						}
					}
				}
			}
		}
	}
	// search drugs on the map environment
	if (lastItem || (!drugWasUse && noInvenDrug)) {
		do {
			long result = 0;
			if (!lastItem) {
				lastItem = fo::func::ai_search_environ(source, fo::ItemType::item_type_drug);
				result = imp_ai::AIInventory::ai_search_environ_corpse_drug(source, fo::ItemType::item_type_drug, noInvenDrug, lastItem);
			}
			if (!lastItem) {
				lastItem = fo::func::ai_search_environ(source, fo::ItemType::item_type_misc_item);
				//result = imp_ai::AIInventory::ai_search_environ_corpse_drug(source, fo::ItemType::item_type_misc_item, noInvenDrug, lastItem);
			}
			if (!result && lastItem) lastItem = fo::func::ai_retrieve_object(source, lastItem);

			// [FIX] Prevent the use of healing drugs when not necessary
			// noInvenDrug: is set to 2 that healing is required
			if (lastItem && noInvenDrug != 2 && game::Items::IsHealingItem(lastItem)) {
				long maxHP = fo::func::stat_level(source, fo::Stat::STAT_max_hit_points);
				if (10 + source->critter.health >= maxHP) { // quick check current HP
					return; // exit: don't use healing item
				}
			}

			if (lastItem && !game::Items::item_remove_mult(source, lastItem, 1, 0x4286F0)) { // [HOOK_REMOVEINVENOBJ]
				if (!game::Items::UseDrugItemFunc(source, lastItem)) lastItem = nullptr;     // [HOOK_USEOBJON]

				source->critter.decreaseAP(aiUseItemAPCost);
			}
		} while (lastItem && source->critter.getAP() >= aiUseItemAPCost);
	}
}

static void __declspec(naked) ai_check_drugs_replacement() {
	__asm {
		push ecx;
		push eax; // source
		call game::CombatAI::ai_check_drugs;
		pop  ecx;
		xor  eax, eax;
		retn;
	}
}

static void __declspec(naked) ai_can_use_drug_hack() {
	__asm {
		push ecx; // item
		call game::Items::IsHealingItem;
		pop  ecx;
		test al, al;
		retn;
	}
}

void CombatAI::init() {

	// Replacing the ai_check_drugs_ function for checking healing items and code fixes
	sf::MakeJump(fo::funcoffs::ai_check_drugs_, ai_check_drugs_replacement); // 0x428480

	// Changing code ai_can_use_drug_ function to check the healing items
	sf::MakeCall(0x429BDE, ai_can_use_drug_hack, 6);
	sf::SafeWrite8(0x429BE9, sf::CodeType::JumpNZ);    // jz > jnz
	sf::SafeWrite8(0x429BF1, sf::CodeType::JumpShort); // jnz > jmp

	drugUsePerfFixMode = sf::IniReader::GetConfigInt("Misc", "AIDrugUsePerfFix", 0);
	if (drugUsePerfFixMode > 0) sf::dlogr("Applying AI drug use preference fix.", DL_FIX);
}

}