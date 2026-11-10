#include "BattleNetwork.h"
#include "Engine/NetSerialization.h"

bool FBattleNetUnit::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
	// Centimeter world, 0.1 cm transport precision. Authoritative positions remain full precision.
	uint16 WireID = static_cast<uint16>(ID), WireSquad = static_cast<uint16>(Squad);
	uint8 WireTeam = static_cast<uint8>(Team), HasPrivate = Squad != INDEX_NONE;
	Ar << WireID; Ar.SerializeBits(&WireTeam, 1); Ar.SerializeBits(&HasPrivate, 1); Ar << HP;
	FVector_NetQuantize10 WirePosition(Position); bool bPositionOK = false;
	WirePosition.NetSerialize(Ar, Map, bPositionOK);
	uint16 Yaw = FRotator::CompressAxisToShort(Rotation.Yaw); Ar << Yaw;
	bool bDestinationOK = true;
	if (HasPrivate)
	{
		Ar << WireSquad;
		uint8 WireAmmo = static_cast<uint8>(Ammo), WireReserve = static_cast<uint8>(Reserve), WireOrder = static_cast<uint8>(Order);
		Ar << WireAmmo; Ar << WireReserve; Ar.SerializeBits(&WireOrder, 2);
		uint32 WireShots = static_cast<uint32>(ShotSequence); Ar.SerializeIntPacked(WireShots);
		// Ceil preserves a positive in-progress reload until authoritative completion.
		uint16 ReloadMS = static_cast<uint16>(FMath::Clamp(FMath::CeilToInt(ReloadRemaining * 1000), 0, 65535)); Ar << ReloadMS;
		FVector_NetQuantize10 WireDestination(Destination); WireDestination.NetSerialize(Ar, Map, bDestinationOK);
		if (Ar.IsLoading()) { Squad = WireSquad; Ammo = WireAmmo; Reserve = WireReserve; Order = static_cast<EBattleOrder>(WireOrder); ShotSequence = WireShots; ReloadRemaining = ReloadMS * .001f; Destination = WireDestination; }
	}
	else if (Ar.IsLoading())
	{
		Squad = INDEX_NONE; Ammo = Reserve = ShotSequence = 0; ReloadRemaining = 0; Order = EBattleOrder::Hold; Destination = FVector::ZeroVector;
	}
	if (Ar.IsLoading()) { ID = WireID; Team = WireTeam; Position = WirePosition; Rotation = FRotator(0, FRotator::DecompressAxisFromShort(Yaw), 0); }
	bOutSuccess = bPositionOK && bDestinationOK && !Ar.IsError(); return true;
}
