/**************************************************************************************
Copyright 2015 Applied Research Associates, Inc.
Licensed under the Apache License, Version 2.0 (the "License"); you may not use
this file except in compliance with the License. You may obtain a copy of the License
at:
http://www.apache.org/licenses/LICENSE-2.0
Unless required by applicable law or agreed to in writing, software distributed under
the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
CONDITIONS OF ANY KIND, either express or implied. See the License for the
specific language governing permissions and limitations under the License.
**************************************************************************************/
#include <biogears/engine/Systems/BloodChemistry.h>

#include <biogears/cdm/compartment/substances/SELiquidSubstanceQuantity.h>
#include <biogears/cdm/patient/SEPatient.h>
#include <biogears/cdm/patient/assessments/SECompleteBloodCount.h>
#include <biogears/cdm/patient/assessments/SEComprehensiveMetabolicPanel.h>
#include <biogears/cdm/properties/SEScalarAmountPerTime.h>
#include <biogears/cdm/properties/SEScalarAmountPerVolume.h>
#include <biogears/cdm/properties/SEScalarFraction.h>
#include <biogears/cdm/properties/SEScalarFrequency.h>
#include <biogears/cdm/properties/SEScalarHeatCapacitancePerMass.h>
#include <biogears/cdm/properties/SEScalarMass.h>
#include <biogears/cdm/properties/SEScalarMassPerAmount.h>
#include <biogears/cdm/properties/SEScalarMassPerAreaTime.h>
#include <biogears/cdm/properties/SEScalarMassPerVolume.h>
#include <biogears/cdm/properties/SEScalarPressure.h>
#include <biogears/cdm/properties/SEScalarVolume.h>
#include <biogears/cdm/properties/SEScalarVolumePerTime.h>
#include <biogears/cdm/system/physiology/SECardiovascularSystem.h>
#include <biogears/cdm/system/physiology/SEDrugSystem.h>
#include <biogears/engine/BioGearsPhysiologyEngine.h>
#include <biogears/engine/Controller/BioGears.h>
namespace BGE = mil::tatrc::physiology::biogears;

#pragma warning(disable : 4786)
#pragma warning(disable : 4275)
namespace biogears {
BloodChemistry::BloodChemistry(BioGears& bg)
  : SEBloodChemistrySystem(bg.GetLogger())
  , m_data(bg)
{
  Clear();
}

BloodChemistry::~BloodChemistry()
{
  Clear();
}

void BloodChemistry::Clear()
{
  SEBloodChemistrySystem::Clear();
  m_aorta = nullptr;
  m_aortaO2 = nullptr;
  m_aortaCO2 = nullptr;
  m_aortaCO = nullptr;
  m_aortaBicarbonate = nullptr;
  m_brainO2 = nullptr;
  m_myocardiumO2 = nullptr;
  m_pulmonaryArteriesO2 = nullptr;
  m_pulmonaryArteriesCO2 = nullptr;
  m_pulmonaryVeinsO2 = nullptr;
  m_pulmonaryVeinsCO2 = nullptr;
  m_venaCava = nullptr;
  m_venaCavaO2 = nullptr;
  m_venaCavaCO2 = nullptr;
  m_venaCavaKetones = nullptr;
  m_venaCavaAlbumin = nullptr;
  m_venaCavaBicarbonate = nullptr;
  m_venaCavaCalcium = nullptr;
  m_venaCavaChloride = nullptr;
  m_venaCavaCreatinine = nullptr;
  m_venaCavaEpinephrine = nullptr;
  m_venaCavaGlucose = nullptr;
  m_venaCavaInsulin = nullptr;
  m_venaCavaLactate = nullptr;
  m_venaCavaPotassium = nullptr;
  m_venaCavaRBC = nullptr;
  m_venaCavaSodium = nullptr;
  m_venaCavaTriacylglycerol = nullptr;
  m_venaCavaUrea = nullptr;
  m_ArterialOxygen_mmHg.Reset();
  m_ArterialCarbonDioxide_mmHg.Reset();

  m_donorRBC = 0.0;
  m_patientRBC = 0.0;
  m_2Agglutinate = 0.0;
  m_p3Agglutinate = 0.0;
  m_d3Agglutinate = 0.0;
  m_4Agglutinate = 0.0;
  m_RemovedRBC = 0.0;
}

//--------------------------------------------------------------------------------------------------
/// \brief
/// Initializes system properties.
//--------------------------------------------------------------------------------------------------
void BloodChemistry::Initialize()
{
  BioGearsSystem::Initialize();
  GetBloodDensity().SetValue(1050, MassPerVolumeUnit::kg_Per_m3);
  GetBloodSpecificHeat().SetValue(3617, HeatCapacitancePerMassUnit::J_Per_K_kg);
  GetVolumeFractionNeutralLipidInPlasma().SetValue(0.0023);
  GetVolumeFractionNeutralPhospholipidInPlasma().SetValue(0.0013);
  GetPhosphate().SetValue(1.1, AmountPerVolumeUnit::mmol_Per_L);
  GetStrongIonDifference().SetValue(40.5, AmountPerVolumeUnit::mmol_Per_L);
  GetTotalBilirubin().SetValue(0.70, MassPerVolumeUnit::mg_Per_dL); //Reference range is 0.2-1.0
  //Note that RedBloodCellAcetylcholinesterase is initialized in Drugs file because Drugs is processed before Blood Chemistry
  GetInflammatoryResponse().Initialize();
  m_ArterialOxygen_mmHg.Sample(m_aortaO2->GetPartialPressure(PressureUnit::mmHg));
  m_ArterialCarbonDioxide_mmHg.Sample(m_aortaCO2->GetPartialPressure(PressureUnit::mmHg));
  GetCarbonMonoxideSaturation().SetValue(0);

  m_RhFactorMismatch = 0.0; // Only matters when patient is negative type
  //m_data.GetDrugs().GetTransfusionReactionVolume().SetValue(0.0, VolumeUnit::uL);
  m_donorRBC = 0.0;
  m_patientRBC = m_venaCava->GetSubstanceQuantity(m_data.GetSubstances().GetRBC())->GetMolarity(AmountPerVolumeUnit::ct_Per_uL) * m_data.GetCardiovascular().GetBloodVolume(VolumeUnit::uL);
  m_2Agglutinate = 0.0;
  m_p3Agglutinate = 0.0;
  m_d3Agglutinate = 0.0;
  m_4Agglutinate = 0.0;
  m_RemovedRBC = 0.0;

  Process(); // Calculate the initial system values
}

bool BloodChemistry::Load(const CDM::BioGearsBloodChemistrySystemData& in)
{
  if (!SEBloodChemistrySystem::Load(in))
    return false;
  m_ArterialOxygen_mmHg.Load(in.ArterialOxygenAverage_mmHg());
  m_ArterialCarbonDioxide_mmHg.Load(in.ArterialCarbonDioxideAverage_mmHg());

  BioGearsSystem::LoadState();

  return true;
}
CDM::BioGearsBloodChemistrySystemData* BloodChemistry::Unload() const
{
  CDM::BioGearsBloodChemistrySystemData* data = new CDM::BioGearsBloodChemistrySystemData();
  Unload(*data);
  return data;
}
void BloodChemistry::Unload(CDM::BioGearsBloodChemistrySystemData& data) const
{
  SEBloodChemistrySystem::Unload(data);
  data.ArterialOxygenAverage_mmHg(std::unique_ptr<CDM::RunningAverageData>(m_ArterialOxygen_mmHg.Unload()));
  data.ArterialCarbonDioxideAverage_mmHg(std::unique_ptr<CDM::RunningAverageData>(m_ArterialCarbonDioxide_mmHg.Unload()));
}

//--------------------------------------------------------------------------------------------------
/// \brief
/// Initializes parameters for BloodChemistry Class
///
///  \details
///  Initializes member variables and system level values on the common data model.
//--------------------------------------------------------------------------------------------------
void BloodChemistry::SetUp()
{
  m_PatientActions = &m_data.GetActions().GetPatientActions();
  m_Patient = &m_data.GetPatient();

  const BioGearsConfiguration& ConfigData = m_data.GetConfiguration();
  m_redBloodCellVolume_mL = ConfigData.GetMeanCorpuscularVolume(VolumeUnit::mL);
  m_HbPerRedBloodCell_ug_Per_ct = ConfigData.GetMeanCorpuscularHemoglobin(MassPerAmountUnit::ug_Per_ct);
  m_HbLostToUrine = 0.0;
  m_RhFactorMismatch = 0.0; // Only matters when patient is negative type

  //Substance
  SESubstance* albumin = &m_data.GetSubstances().GetAlbumin();
  SESubstance* aminoAcids = &m_data.GetSubstances().GetAminoAcids();
  SESubstance* bicarbonate = &m_data.GetSubstances().GetBicarbonate();
  SESubstance* calcium = &m_data.GetSubstances().GetCalcium();
  SESubstance* chloride = &m_data.GetSubstances().GetChloride();
  SESubstance* creatinine = &m_data.GetSubstances().GetCreatinine();
  SESubstance* epinephrine = &m_data.GetSubstances().GetEpi();
  SESubstance* glucagon = &m_data.GetSubstances().GetGlucagon();
  SESubstance* glucose = &m_data.GetSubstances().GetGlucose();
  SESubstance* insulin = &m_data.GetSubstances().GetInsulin();
  SESubstance* ketones = &m_data.GetSubstances().GetKetones();
  SESubstance* lactate = &m_data.GetSubstances().GetLactate();
  SESubstance* potassium = &m_data.GetSubstances().GetPotassium();
  SESubstance* rbc = &m_data.GetSubstances().GetRBC();
  SESubstance* sodium = &m_data.GetSubstances().GetSodium();
  SESubstance* triaclyglycerol = &m_data.GetSubstances().GetTriacylglycerol();
  SESubstance* urea = &m_data.GetSubstances().GetUrea();

  m_aorta = m_data.GetCompartments().GetLiquidCompartment(BGE::VascularCompartment::Aorta);
  m_aortaO2 = m_aorta->GetSubstanceQuantity(m_data.GetSubstances().GetO2());
  m_aortaCO2 = m_aorta->GetSubstanceQuantity(m_data.GetSubstances().GetCO2());
  m_aortaBicarbonate = m_aorta->GetSubstanceQuantity(m_data.GetSubstances().GetHCO3());

  m_brainO2 = m_data.GetCompartments().GetLiquidCompartment(BGE::VascularCompartment::Brain)->GetSubstanceQuantity(m_data.GetSubstances().GetO2());
  m_myocardiumO2 = m_data.GetCompartments().GetLiquidCompartment(BGE::VascularCompartment::Myocardium)->GetSubstanceQuantity(m_data.GetSubstances().GetO2());

  m_venaCava = m_data.GetCompartments().GetLiquidCompartment(BGE::VascularCompartment::VenaCava);
  m_venaCavaO2 = m_venaCava->GetSubstanceQuantity(m_data.GetSubstances().GetO2());
  m_venaCavaCO2 = m_venaCava->GetSubstanceQuantity(m_data.GetSubstances().GetCO2());
  m_venaCavaAlbumin = m_venaCava->GetSubstanceQuantity(*albumin);
  m_venaCavaAminoAcids = m_venaCava->GetSubstanceQuantity(*aminoAcids);
  m_venaCavaBicarbonate = m_venaCava->GetSubstanceQuantity(*bicarbonate);
  m_venaCavaCalcium = m_venaCava->GetSubstanceQuantity(*calcium);
  m_venaCavaChloride = m_venaCava->GetSubstanceQuantity(*chloride);
  m_venaCavaCreatinine = m_venaCava->GetSubstanceQuantity(*creatinine);
  m_venaCavaEpinephrine = m_venaCava->GetSubstanceQuantity(*epinephrine);
  m_venaCavaGlucagon = m_venaCava->GetSubstanceQuantity(*glucagon);
  m_venaCavaGlucose = m_venaCava->GetSubstanceQuantity(*glucose);
  m_venaCavaInsulin = m_venaCava->GetSubstanceQuantity(*insulin);
  m_venaCavaKetones = m_venaCava->GetSubstanceQuantity(*ketones);
  m_venaCavaLactate = m_venaCava->GetSubstanceQuantity(*lactate);
  m_venaCavaPotassium = m_venaCava->GetSubstanceQuantity(*potassium);
  m_venaCavaRBC = m_venaCava->GetSubstanceQuantity(*rbc);
  m_venaCavaSodium = m_venaCava->GetSubstanceQuantity(*sodium);
  m_venaCavaTriacylglycerol = m_venaCava->GetSubstanceQuantity(*triaclyglycerol);
  m_venaCavaUrea = m_venaCava->GetSubstanceQuantity(*urea);

  SELiquidCompartment* pulmonaryArteries = m_data.GetCompartments().GetLiquidCompartment(BGE::VascularCompartment::PulmonaryArteries);
  m_pulmonaryArteriesO2 = pulmonaryArteries->GetSubstanceQuantity(m_data.GetSubstances().GetO2());
  m_pulmonaryArteriesCO2 = pulmonaryArteries->GetSubstanceQuantity(m_data.GetSubstances().GetCO2());

  SELiquidCompartment* pulmonaryVeins = m_data.GetCompartments().GetLiquidCompartment(BGE::VascularCompartment::PulmonaryVeins);
  m_pulmonaryVeinsO2 = pulmonaryVeins->GetSubstanceQuantity(m_data.GetSubstances().GetO2());
  m_pulmonaryVeinsCO2 = pulmonaryVeins->GetSubstanceQuantity(m_data.GetSubstances().GetCO2());

  double dT_s = m_data.GetTimeStep().GetValue(TimeUnit::s);
  m_PatientActions = &m_data.GetActions().GetPatientActions();

  m_InflammatoryResponse = &GetInflammatoryResponse();
}

void BloodChemistry::AtSteadyState()
{
}

//--------------------------------------------------------------------------------------------------
/// \brief
/// Blood Chemistry Preprocess method
///
/// \details
/// The current BioGears implementation only checks for active Sepsis event.
//--------------------------------------------------------------------------------------------------
void BloodChemistry::PreProcess()
{
  InflammatoryResponse();
}

//--------------------------------------------------------------------------------------------------
/// \brief
/// Blood Chemistry process method
///
/// \details
/// The system data (blood concentrations, blood gases, and other blood properties) that are calculated
/// or changed by other systems are set on the blood chemistry system data objects. Events
/// are triggered at specific blood concentrations of certain substances in CheckBloodGasLevels().
//--------------------------------------------------------------------------------------------------
void BloodChemistry::Process()
{
  //Push the compartment values of O2 and CO2 partial pressures on the corresponding system data.
  GetOxygenSaturation().Set(m_aortaO2->GetSaturation());
  GetOxygenVenousSaturation().Set(m_venaCavaO2->GetSaturation());
  GetCarbonDioxideSaturation().Set(m_aortaCO2->GetSaturation());
  if (m_aortaCO == nullptr && m_data.GetSubstances().IsActive(m_data.GetSubstances().GetCO()))
    m_aortaCO = m_aorta->GetSubstanceQuantity(m_data.GetSubstances().GetCO());
  if (m_aortaCO != nullptr) {
    GetCarbonMonoxideSaturation().Set(m_aortaCO->GetSaturation());
    GetPulseOximetry().SetValue(GetOxygenSaturation().GetValue() + GetCarbonMonoxideSaturation().GetValue());
  } else {
    GetPulseOximetry().Set(GetOxygenSaturation());
  }

  // This Hemoglobin Content is the mass of the hemoglobin only, not the hemoglobin and bound gas.
  // So we have to take our 4 Hb species masses and remove the mass of the gas.
  // Step 1) Get the mass of the bound species, which includes the mass of the bound gas.
  // Step 2) Convert to moles using the molar weight of the bound species (molar mass of bound species includes the mass of the bound gas and the mass of the unbound species).
  // Step 3) Covert moles of the bound species to moles of the unbound species. i.e.multiply by 1 (this step is implied)
  // Step 4) Convert moles of the unbound species to mass in order to get a total mass of hemoglobin (as opposed to a total mass of hemoglobin plus the bound gases).
  double totalHb_g = m_data.GetSubstances().GetSubstanceMass(m_data.GetSubstances().GetHb(), m_data.GetCompartments().GetVascularLeafCompartments(), MassUnit::g);
  double totalHbO2_g = (m_data.GetSubstances().GetSubstanceMass(m_data.GetSubstances().GetHbO2(), m_data.GetCompartments().GetVascularLeafCompartments(), MassUnit::g) / m_data.GetSubstances().GetHbO2().GetMolarMass(MassPerAmountUnit::g_Per_mol)) * m_data.GetSubstances().GetHb().GetMolarMass(MassPerAmountUnit::g_Per_mol);
  double totalHbCO2_g = (m_data.GetSubstances().GetSubstanceMass(m_data.GetSubstances().GetHbCO2(), m_data.GetCompartments().GetVascularLeafCompartments(), MassUnit::g) / m_data.GetSubstances().GetHbCO2().GetMolarMass(MassPerAmountUnit::g_Per_mol)) * m_data.GetSubstances().GetHb().GetMolarMass(MassPerAmountUnit::g_Per_mol);
  double totalHBO2CO2_g = (m_data.GetSubstances().GetSubstanceMass(m_data.GetSubstances().GetHbO2CO2(), m_data.GetCompartments().GetVascularLeafCompartments(), MassUnit::g) / m_data.GetSubstances().GetHbO2CO2().GetMolarMass(MassPerAmountUnit::g_Per_mol)) * m_data.GetSubstances().GetHb().GetMolarMass(MassPerAmountUnit::g_Per_mol);
  double totalHBCO_g = 0.0;
  if (m_aortaCO != nullptr)
    totalHBCO_g = (m_data.GetSubstances().GetSubstanceMass(m_data.GetSubstances().GetHbCO(), m_data.GetCompartments().GetVascularLeafCompartments(), MassUnit::g) / m_data.GetSubstances().GetHbCO().GetMolarMass(MassPerAmountUnit::g_Per_mol)) * m_data.GetSubstances().GetHb().GetMolarMass(MassPerAmountUnit::g_Per_mol);

  double totalHemoglobinO2Hemoglobin_g = totalHb_g + totalHbO2_g + totalHbCO2_g + totalHBO2CO2_g + totalHBCO_g;
  GetHemoglobinContent().SetValue(totalHemoglobinO2Hemoglobin_g, MassUnit::g);
  m_HbLostToUrine = GetHemoglobinContent().GetValue(MassUnit::g);

  // Calculate Blood Cell Counts
  SESubstance& rbc = m_data.GetSubstances().GetRBC();
  double TotalBloodVolume_mL = m_data.GetCardiovascular().GetBloodVolume(VolumeUnit::mL);

  double RedBloodCellCount_ct_Per_uL = m_venaCavaRBC->GetMolarity(AmountPerVolumeUnit::ct_Per_uL);
  double RedBloodCellCount_ct = (RedBloodCellCount_ct_Per_uL)*TotalBloodVolume_mL * 1000;
  //double RedBloodCellCount_ct = GetHemoglobinContent(MassUnit::ug) / m_HbPerRedBloodCell_ug_Per_ct;
  double RedBloodCellVolume_mL = RedBloodCellCount_ct * m_redBloodCellVolume_mL;
  GetHematocrit().SetValue((RedBloodCellVolume_mL / TotalBloodVolume_mL));
  // Yes, we are giving GetRedBloodCellCount a concentration, because that is what it is, but clinically, it is known as RedBloodCellCount
  GetRedBloodCellCount().SetValue(RedBloodCellCount_ct / m_data.GetCardiovascular().GetBloodVolume(VolumeUnit::L), AmountPerVolumeUnit::ct_Per_L);
  GetPlasmaVolume().SetValue(TotalBloodVolume_mL - RedBloodCellVolume_mL, VolumeUnit::mL); //Look into changing to account for platelets and wbc too

  // Concentrations
  double albuminConcentration_ug_Per_mL = m_venaCavaAlbumin->GetConcentration(MassPerVolumeUnit::ug_Per_mL);
  m_data.GetSubstances().GetAlbumin().GetBloodConcentration().Set(m_venaCavaAlbumin->GetConcentration());
  m_data.GetSubstances().GetAminoAcids().GetBloodConcentration().Set(m_venaCavaAminoAcids->GetConcentration());
  m_data.GetSubstances().GetBicarbonate().GetBloodConcentration().Set(m_venaCavaBicarbonate->GetConcentration());
  GetBloodUreaNitrogenConcentration().SetValue(m_venaCavaUrea->GetConcentration(MassPerVolumeUnit::ug_Per_mL) / 2.14, MassPerVolumeUnit::ug_Per_mL);
  m_data.GetSubstances().GetCalcium().GetBloodConcentration().Set(m_venaCavaCalcium->GetConcentration());
  m_data.GetSubstances().GetChloride().GetBloodConcentration().Set(m_venaCavaChloride->GetConcentration());
  m_data.GetSubstances().GetCreatinine().GetBloodConcentration().Set(m_venaCavaCreatinine->GetConcentration());
  m_data.GetSubstances().GetEpi().GetBloodConcentration().Set(m_venaCavaEpinephrine->GetConcentration());
  m_data.GetSubstances().GetGlobulin().GetBloodConcentration().SetValue(albuminConcentration_ug_Per_mL * 1.6 - albuminConcentration_ug_Per_mL, MassPerVolumeUnit::ug_Per_mL); // 1.6 comes from reading http://www.drkaslow.com/html/proteins_-_albumin-_globulins-_etc.html
  m_data.GetSubstances().GetGlucagon().GetBloodConcentration().Set(m_venaCavaGlucagon->GetConcentration());
  m_data.GetSubstances().GetGlucose().GetBloodConcentration().Set(m_venaCavaGlucose->GetConcentration());
  double HemoglobinConcentration = totalHemoglobinO2Hemoglobin_g / TotalBloodVolume_mL;
  m_data.GetSubstances().GetHb().GetBloodConcentration().SetValue(HemoglobinConcentration, MassPerVolumeUnit::g_Per_mL);
  m_data.GetSubstances().GetInsulin().GetBloodConcentration().Set(m_venaCavaInsulin->GetConcentration());
  m_data.GetSubstances().GetKetones().GetBloodConcentration().Set(m_venaCavaKetones->GetConcentration());
  m_data.GetSubstances().GetLactate().GetBloodConcentration().Set(m_venaCavaLactate->GetConcentration());
  m_data.GetSubstances().GetPotassium().GetBloodConcentration().Set(m_venaCavaPotassium->GetConcentration());
  m_data.GetSubstances().GetRBC().GetBloodConcentration().Set(m_venaCavaRBC->GetConcentration());
  m_data.GetSubstances().GetSodium().GetBloodConcentration().Set(m_venaCavaSodium->GetConcentration());
  GetTotalProteinConcentration().SetValue(albuminConcentration_ug_Per_mL * 1.6, MassPerVolumeUnit::ug_Per_mL);
  m_data.GetSubstances().GetTriacylglycerol().GetBloodConcentration().Set(m_venaCavaTriacylglycerol->GetConcentration());
  m_data.GetSubstances().GetUrea().GetBloodConcentration().Set(m_venaCavaUrea->GetConcentration());

  double otherIons_mmol_Per_L = -5.4; //Na, K, and Cl baseline concentrations give SID = 45.83, we assume baseline SID = 40.5, thus "other ions" (i.e. Mg, Ca, Ketones) make up -5.3 mmol_Per_L equivalent of charge
  double strongIonDifference_mmol_Per_L = m_venaCavaSodium->GetMolarity(AmountPerVolumeUnit::mmol_Per_L) + m_venaCavaPotassium->GetMolarity(AmountPerVolumeUnit::mmol_Per_L) - (m_venaCavaChloride->GetMolarity(AmountPerVolumeUnit::mmol_Per_L) + (m_venaCavaLactate->GetMolarity(AmountPerVolumeUnit::mmol_Per_L))) + otherIons_mmol_Per_L;
  //GetStrongIonDifference().SetValue(strongIonDifference_mmol_Per_L, AmountPerVolumeUnit::mmol_Per_L);

  // Calculate pH
  GetArterialBloodPH().Set(m_aorta->GetPH());
  GetVenousBloodPH().Set(m_venaCava->GetPH());

  // Pressures
  // arterial gas partial pressures -
  GetArterialOxygenPressure().Set(m_aortaO2->GetPartialPressure());
  GetArterialCarbonDioxidePressure().Set(m_aortaCO2->GetPartialPressure());
  // pulmonary arteries
  GetPulmonaryArterialOxygenPressure().Set(m_pulmonaryArteriesO2->GetPartialPressure());
  GetPulmonaryArterialCarbonDioxidePressure().Set(m_pulmonaryArteriesCO2->GetPartialPressure());
  // pulmonary vein gas partial pressures -  the average of right and left pulmonary vein gas pressures
  GetPulmonaryVenousOxygenPressure().Set(m_pulmonaryVeinsO2->GetPartialPressure());
  GetPulmonaryVenousCarbonDioxidePressure().Set(m_pulmonaryVeinsCO2->GetPartialPressure());
  // venous gas partial pressures
  GetVenousOxygenPressure().Set(m_venaCavaO2->GetPartialPressure());
  GetVenousCarbonDioxidePressure().Set(m_venaCavaCO2->GetPartialPressure());

  double totalFlow_mL_Per_min = m_data.GetCardiovascular().GetCardiacOutput(VolumePerTimeUnit::mL_Per_min);
  double shuntFlow_mL_Per_min = m_data.GetCardiovascular().GetPulmonaryMeanShuntFlow(VolumePerTimeUnit::mL_Per_min);
  double shunt = shuntFlow_mL_Per_min / totalFlow_mL_Per_min;
  GetShuntFraction().SetValue(shunt);

  //-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
  //Check for HTR
  //SESubstance* m_AntigenA = m_data.GetSubstances().GetSubstance("Antigen_A");
  //SESubstance* m_AntigenB = m_data.GetSubstances().GetSubstance("Antigen_B");

  // ABO antigen check. if O blood being given OR AB blood in patient, skip compatibility checks
  //if (m_data.GetPatient().GetBloodType() == (CDM::enumBloodType::AB)) {
  //  return;
  //} else if ((m_data.GetPatient().GetBloodType() == (CDM::enumBloodType::O) && ((m_data.GetSubstances().GetAntigen_A().GetBloodConcentration(MassPerVolumeUnit::g_Per_mL) > 0) || m_data.GetSubstances().GetAntigen_B().GetBloodConcentration(MassPerVolumeUnit::g_Per_mL) > 0)) //(m_data.GetSubstances().IsActive(*m_AntigenA) || m_data.GetSubstances().IsActive(*m_AntigenB)))
  //           || ((m_venaCava->GetSubstanceQuantity(*m_AntigenA)->GetMolarity(AmountPerVolumeUnit::ct_Per_uL) > 0) && (m_venaCava->GetSubstanceQuantity(*m_AntigenB)->GetMolarity(AmountPerVolumeUnit::ct_Per_uL) > 0))) { //(m_data.GetSubstances().IsActive(*m_AntigenA) && m_data.GetSubstances().IsActive(*m_AntigenB)))
  //  CalculateHemolyticTransfusionReaction();
  //} /*else if ((m_data.GetPatient().GetBloodRh() == (CDM::enumBinaryResults::negative)) && (m_data.GetS == (CDM::enumBool::positive))) {
  //}   */ /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////HELP HERE///////////////////////

  if (m_data.GetPatient().IsEventActive(CDM::enumPatientEvent::HemolyticTransfusionReaction)) {
    if (m_data.GetDrugs().GetTransfusionReactionVolume().GetValue(VolumeUnit::uL) > 0) {
      CalculateHemolyticTransfusionReaction(true);
    } else {
      CalculateHemolyticTransfusionReaction();
    }
  }

  m_data.GetDataTrack().Probe("pRBC1", m_patientRBC);
  m_data.GetDataTrack().Probe("dRBC1", m_donorRBC);
  m_data.GetDataTrack().Probe("RBC2", m_2Agglutinate);
  m_data.GetDataTrack().Probe("pRBC3", m_p3Agglutinate);
  m_data.GetDataTrack().Probe("dRBC3", m_d3Agglutinate);
  m_data.GetDataTrack().Probe("RBC4", m_4Agglutinate);

  //dynamic_cast<BloodChemistry&>(m_data.GetBloodChemistry()).CalculateHemolyticTransfusionReaction();

  //-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------

  //Throw events if levels are low/high
  CheckBloodSubstanceLevels();

  // Total up all active substances
  double bloodMass_ug;
  double tissueMass_ug;
  for (SESubstance* sub : m_data.GetSubstances().GetActiveSubstances()) {
    if (sub->GetState() != CDM::enumSubstanceState::Cellular) {
      bloodMass_ug = m_data.GetSubstances().GetSubstanceMass(*sub, m_data.GetCompartments().GetVascularLeafCompartments(), MassUnit::ug);
      tissueMass_ug = m_data.GetSubstances().GetSubstanceMass(*sub, m_data.GetCompartments().GetTissueLeafCompartments(), MassUnit::ug);
      sub->GetMassInBody().SetValue(bloodMass_ug + tissueMass_ug, MassUnit::ug);
      sub->GetMassInBlood().SetValue(bloodMass_ug, MassUnit::ug);
      sub->GetMassInTissue().SetValue(tissueMass_ug, MassUnit::ug);
    }
  }
}

//--------------------------------------------------------------------------------------------------
/// \brief
/// Blood Chemistry postprocess method
///
/// \details
/// The current BioGears implementation has no specific postprocess functionality.
//--------------------------------------------------------------------------------------------------
void BloodChemistry::PostProcess()
{
  m_data.GetDataTrack().Probe("aaaaaaa", GetHemoglobinLostToUrine(MassUnit::g));
  if (m_data.GetActions().GetPatientActions().HasOverride()
      && m_data.GetState() == EngineState::Active) {
    if (m_data.GetActions().GetPatientActions().GetOverride()->HasBloodChemistryOverride()) {
      ProcessOverride();
    }
  }
}

//--------------------------------------------------------------------------------------------------
/// \brief
/// Checks the blood substance (oxygen, carbon dioxide, glucose, etc.) levels and sets events.
///
/// \details
/// Checks the oxygen, carbon dioxide, glucose, ketones, and lactate levels in specific compartments of the body.
/// Set events for Hypercapnia, Hypoxia, Brain and MyoCardium Oxygen Deficit, Hypo/hyperglycemia and related events,
/// ketoacidosis, and lactic acidosis based on the levels.
//--------------------------------------------------------------------------------------------------
void BloodChemistry::CheckBloodSubstanceLevels()
{
  SEPatient& patient = m_data.GetPatient();
  double hypoxiaFlag = 65.0; //Arterial O2 Partial Pressure in mmHg \cite Pierson2000Pathophysiology
  double hypoxiaIrreversible = 15.0; // \cite hobler1973HypoxemiaCardiacOutput
  double hypoglycemiaLevel_mg_Per_dL = 70;
  double hypoglycemicShockLevel_mg_Per_dL = 50;
  double hypoglycemicComaLevel_mg_Per_dL = 20;
  double hyperglycemiaLevel_mg_Per_dL = 200;
  double lacticAcidosisLevel_mg_Per_dL = 44;
  double ketoacidosisLevel_mg_Per_dL = 122;

  m_ArterialOxygen_mmHg.Sample(m_aortaO2->GetPartialPressure(PressureUnit::mmHg));
  m_ArterialCarbonDioxide_mmHg.Sample(m_aortaCO2->GetPartialPressure(PressureUnit::mmHg));
  //Only check these at the end of a cardiac cycle and reset at start of cardiac cycle
  if (patient.IsEventActive(CDM::enumPatientEvent::StartOfCardiacCycle)) {
    double arterialOxygen_mmHg = m_ArterialOxygen_mmHg.Value();
    double arterialCarbonDioxide_mmHg = m_ArterialCarbonDioxide_mmHg.Value();

    if (m_data.GetState() > EngineState::InitialStabilization) { // Don't throw events if we are initializing
      // hypercapnia check
      double hypercapniaFlag = 60.0; // \cite Guyton11thEd p.531
      double carbonDioxideToxicity = 80.0; // \cite labertsen1971CarbonDioxideToxicity
      if (arterialCarbonDioxide_mmHg >= hypercapniaFlag) {
        /// \event Patient: Hypercapnia. The carbon dioxide partial pressure has risen above 60 mmHg. The patient is now hypercapnic.
        patient.SetEvent(CDM::enumPatientEvent::Hypercapnia, true, m_data.GetSimulationTime());

        if (arterialCarbonDioxide_mmHg > carbonDioxideToxicity) {
          m_ss << "Arterial Carbon Dioxide partial pressure is " << arterialCarbonDioxide_mmHg << ". This is beyond 80 mmHg triggering extreme Hypercapnia, patient is in an irreversible state.";
          Warning(m_ss);
          /// \irreversible The carbon dioxide partial pressure is greater than 80 mmHg.
          if (!m_PatientActions->HasOverride()) {
            patient.SetEvent(CDM::enumPatientEvent::IrreversibleState, true, m_data.GetSimulationTime());
          } else {
            if (m_PatientActions->GetOverride()->GetOverrideConformance() == CDM::enumOnOff::On) {
              patient.SetEvent(CDM::enumPatientEvent::IrreversibleState, true, m_data.GetSimulationTime());
            }
          }
        }
      } else if (patient.IsEventActive(CDM::enumPatientEvent::Hypercapnia) && arterialCarbonDioxide_mmHg < (hypercapniaFlag - 3)) {
        /// \event Patient: End Hypercapnia. The carbon dioxide partial pressure has fallen below 57 mmHg. The patient is no longer considered to be hypercapnic.
        /// This event is triggered if the patient was hypercapnic and is now considered to be recovered.
        patient.SetEvent(CDM::enumPatientEvent::Hypercapnia, false, m_data.GetSimulationTime());
      }

      // hypoxia check
      if (arterialOxygen_mmHg <= hypoxiaFlag) {
        /// \event Patient: Hypoxia Event. The oxygen partial pressure has fallen below 65 mmHg, indicating that the patient is hypoxic.
        patient.SetEvent(CDM::enumPatientEvent::Hypoxia, true, m_data.GetSimulationTime());

        if (arterialOxygen_mmHg < hypoxiaIrreversible) {
          m_ss << "Arterial Oxygen partial pressure is " << arterialOxygen_mmHg << ". This is below 15 mmHg triggering extreme Hypoxia, patient is in an irreversible state.";
          Warning(m_ss);
          /// \irreversible Arterial oxygen partial pressure has been critically reduced to below 15 mmHg.
          if (!m_PatientActions->HasOverride()) {
            patient.SetEvent(CDM::enumPatientEvent::IrreversibleState, true, m_data.GetSimulationTime());
          } else {
            if (m_PatientActions->GetOverride()->GetOverrideConformance() == CDM::enumOnOff::On) {
              patient.SetEvent(CDM::enumPatientEvent::IrreversibleState, true, m_data.GetSimulationTime());
            }
          }
        }
      } else if (arterialOxygen_mmHg > (hypoxiaFlag + 3)) {
        /// \event Patient: End Hypoxia Event. The oxygen partial pressure has rise above 68 mmHg. If this occurs when the patient is hypoxic, it will reverse the hypoxic event.
        /// The patient is no longer considered to be hypoxic.
        patient.SetEvent(CDM::enumPatientEvent::Hypoxia, false, m_data.GetSimulationTime());
      }

      //glucose checks

      //hypoglycemia
      if (m_venaCavaGlucose->GetConcentration(MassPerVolumeUnit::mg_Per_dL) < hypoglycemiaLevel_mg_Per_dL) {
        patient.SetEvent(CDM::enumPatientEvent::Hypoglycemia, true, m_data.GetSimulationTime());
      } else if (patient.IsEventActive(CDM::enumPatientEvent::Hypoglycemia) && m_venaCavaGlucose->GetConcentration(MassPerVolumeUnit::mg_Per_dL) > hypoglycemiaLevel_mg_Per_dL + 5) {
        patient.SetEvent(CDM::enumPatientEvent::Hypoglycemia, false, m_data.GetSimulationTime());
      }

      //hypoglycemic shock
      if (m_venaCavaGlucose->GetConcentration(MassPerVolumeUnit::mg_Per_dL) < hypoglycemicShockLevel_mg_Per_dL) {
        patient.SetEvent(CDM::enumPatientEvent::HypoglycemicShock, true, m_data.GetSimulationTime());
      } else if (patient.IsEventActive(CDM::enumPatientEvent::HypoglycemicShock) && m_venaCavaGlucose->GetConcentration(MassPerVolumeUnit::mg_Per_dL) > hypoglycemicShockLevel_mg_Per_dL + 5) {
        patient.SetEvent(CDM::enumPatientEvent::HypoglycemicShock, false, m_data.GetSimulationTime());
      }

      //hypoglycemic coma
      if (m_venaCavaGlucose->GetConcentration(MassPerVolumeUnit::mg_Per_dL) < hypoglycemicComaLevel_mg_Per_dL) {
        patient.SetEvent(CDM::enumPatientEvent::HypoglycemicComa, true, m_data.GetSimulationTime());
      } else if (patient.IsEventActive(CDM::enumPatientEvent::HypoglycemicComa) && m_venaCavaGlucose->GetConcentration(MassPerVolumeUnit::mg_Per_dL) > hypoglycemicComaLevel_mg_Per_dL + 3) {
        patient.SetEvent(CDM::enumPatientEvent::HypoglycemicComa, false, m_data.GetSimulationTime());
      }

      //hyperglycemia
      if (m_venaCavaGlucose->GetConcentration(MassPerVolumeUnit::mg_Per_dL) > hyperglycemiaLevel_mg_Per_dL) {
        patient.SetEvent(CDM::enumPatientEvent::Hyperglycemia, true, m_data.GetSimulationTime());
      } else if (patient.IsEventActive(CDM::enumPatientEvent::Hyperglycemia) && m_venaCavaGlucose->GetConcentration(MassPerVolumeUnit::mg_Per_dL) < hypoglycemicShockLevel_mg_Per_dL - 10) {
        patient.SetEvent(CDM::enumPatientEvent::Hyperglycemia, false, m_data.GetSimulationTime());
      }

      //lactate check
      if (m_venaCavaLactate->GetConcentration(MassPerVolumeUnit::mg_Per_dL) > lacticAcidosisLevel_mg_Per_dL) {
        patient.SetEvent(CDM::enumPatientEvent::LacticAcidosis, true, m_data.GetSimulationTime());
      } else if (patient.IsEventActive(CDM::enumPatientEvent::LacticAcidosis) && m_venaCavaLactate->GetConcentration(MassPerVolumeUnit::mg_Per_dL) < lacticAcidosisLevel_mg_Per_dL - 5) {
        patient.SetEvent(CDM::enumPatientEvent::LacticAcidosis, false, m_data.GetSimulationTime());
      }

      //ketones check
      if (m_venaCavaKetones->GetConcentration(MassPerVolumeUnit::mg_Per_dL) > ketoacidosisLevel_mg_Per_dL) {
        patient.SetEvent(CDM::enumPatientEvent::Ketoacidosis, true, m_data.GetSimulationTime());
      } else if (patient.IsEventActive(CDM::enumPatientEvent::Ketoacidosis) && m_venaCavaKetones->GetConcentration(MassPerVolumeUnit::mg_Per_dL) < ketoacidosisLevel_mg_Per_dL - 5) {
        patient.SetEvent(CDM::enumPatientEvent::Ketoacidosis, false, m_data.GetSimulationTime());
      }

      //sodium check
      //Mild and severe hyponatremia
      if (m_venaCavaSodium->GetMolarity(AmountPerVolumeUnit::mmol_Per_L) < 136.0) {
        patient.SetEvent(CDM::enumPatientEvent::MildHyponatremia, true, m_data.GetSimulationTime());
      } else if (patient.IsEventActive(CDM::enumPatientEvent::MildHyponatremia) && m_venaCavaSodium->GetMolarity(AmountPerVolumeUnit::mmol_Per_L) > 137.0) {
        patient.SetEvent(CDM::enumPatientEvent::MildHyponatremia, false, m_data.GetSimulationTime());
      }

      if (m_venaCavaSodium->GetMolarity(AmountPerVolumeUnit::mmol_Per_L) < 120.0) {
        patient.SetEvent(CDM::enumPatientEvent::SevereHyponatremia, true, m_data.GetSimulationTime());
      } else if (patient.IsEventActive(CDM::enumPatientEvent::SevereHyponatremia) && m_venaCavaSodium->GetMolarity(AmountPerVolumeUnit::mmol_Per_L) > 121.0) {
        patient.SetEvent(CDM::enumPatientEvent::SevereHyponatremia, false, m_data.GetSimulationTime());
      }
      //Mild and severe hypernatremia
      if (m_venaCavaSodium->GetMolarity(AmountPerVolumeUnit::mmol_Per_L) > 148.0) {
        patient.SetEvent(CDM::enumPatientEvent::MildHypernatremia, true, m_data.GetSimulationTime());
      } else if (patient.IsEventActive(CDM::enumPatientEvent::MildHypernatremia) && m_venaCavaSodium->GetMolarity(AmountPerVolumeUnit::mmol_Per_L) < 147.0) {
        patient.SetEvent(CDM::enumPatientEvent::MildHypernatremia, false, m_data.GetSimulationTime());
      }
      if (m_venaCavaSodium->GetMolarity(AmountPerVolumeUnit::mmol_Per_L) > 160.0) {
        patient.SetEvent(CDM::enumPatientEvent::SevereHypernatremia, true, m_data.GetSimulationTime());
      } else if (patient.IsEventActive(CDM::enumPatientEvent::SevereHypernatremia) && m_venaCavaSodium->GetMolarity(AmountPerVolumeUnit::mmol_Per_L) < 159.0) {
        patient.SetEvent(CDM::enumPatientEvent::SevereHypernatremia, false, m_data.GetSimulationTime());
      }

      //potassium check
      //mild and severe hypokalemia
      if (m_venaCavaPotassium->GetMolarity(AmountPerVolumeUnit::mmol_Per_L) < 3.2) {
        patient.SetEvent(CDM::enumPatientEvent::MildHypokalemia, true, m_data.GetSimulationTime());
      } else if (patient.IsEventActive(CDM::enumPatientEvent::MildHypokalemia) && m_venaCavaSodium->GetMolarity(AmountPerVolumeUnit::mmol_Per_L) > 3.3) {
        patient.SetEvent(CDM::enumPatientEvent::MildHypokalemia, false, m_data.GetSimulationTime());
      }
      if (m_venaCavaPotassium->GetMolarity(AmountPerVolumeUnit::mmol_Per_L) < 2.5) {
        patient.SetEvent(CDM::enumPatientEvent::SevereHypokalemia, true, m_data.GetSimulationTime());
      } else if (patient.IsEventActive(CDM::enumPatientEvent::SevereHypokalemia) && m_venaCavaSodium->GetMolarity(AmountPerVolumeUnit::mmol_Per_L) > 2.6) {
        patient.SetEvent(CDM::enumPatientEvent::SevereHypokalemia, false, m_data.GetSimulationTime());
      }
      //mild and severe hyperkalemia
      if (m_venaCavaPotassium->GetMolarity(AmountPerVolumeUnit::mmol_Per_L) > 5.5) {
        patient.SetEvent(CDM::enumPatientEvent::MildHyperkalemia, true, m_data.GetSimulationTime());
      } else if (patient.IsEventActive(CDM::enumPatientEvent::MildHyperkalemia) && m_venaCavaSodium->GetMolarity(AmountPerVolumeUnit::mmol_Per_L) < 5.6) {
        patient.SetEvent(CDM::enumPatientEvent::MildHyperkalemia, false, m_data.GetSimulationTime());
      }
      if (m_venaCavaPotassium->GetMolarity(AmountPerVolumeUnit::mmol_Per_L) > 6.2) {
        patient.SetEvent(CDM::enumPatientEvent::SevereHyperkalemia, true, m_data.GetSimulationTime());
      } else if (patient.IsEventActive(CDM::enumPatientEvent::SevereHyperkalemia) && m_venaCavaSodium->GetMolarity(AmountPerVolumeUnit::mmol_Per_L) < 6.1) {
        patient.SetEvent(CDM::enumPatientEvent::SevereHyperkalemia, false, m_data.GetSimulationTime());
      }
    }

    m_ArterialOxygen_mmHg.Reset();
    m_ArterialCarbonDioxide_mmHg.Reset();
  }

  if (m_data.GetState() > EngineState::InitialStabilization) { // Don't throw events if we are initializing

    // When the brain oxygen partial pressure is too low, events are triggered and event duration is tracked.
    // If the oxygen tension in the brain remains below the thresholds for the specified amount of time, the body
    // will go into an irreversible state. The threshold values are chosen based on empirical data reviewed in summary in @cite dhawan2011neurointensive
    // and from data presented in @cite purins2012brain and @cite doppenberg1998determination.
    if (m_brainO2->GetPartialPressure(PressureUnit::mmHg) < 21.0) {
      /// \event Patient: Brain Oxygen Deficit Event. The oxygen partial pressure in the brain has dropped to a dangerously low level.
      patient.SetEvent(CDM::enumPatientEvent::BrainOxygenDeficit, true, m_data.GetSimulationTime());

      // Irreversible damage occurs if the deficit has gone on too long
      if (patient.GetEventDuration(CDM::enumPatientEvent::BrainOxygenDeficit, TimeUnit::s) > 1800) {
        m_ss << "Brain Oxygen partial pressure is " << m_brainO2->GetPartialPressure(PressureUnit::mmHg) << " and has been below the danger threshold for " << patient.GetEventDuration(CDM::enumPatientEvent::BrainOxygenDeficit, TimeUnit::s) << " seconds. Damage is irreversible.";
        Warning(m_ss);
        /// \irreversible Brain oxygen pressure has been dangerously low for more than 30 minutes.
        if (!m_PatientActions->HasOverride()) {
          patient.SetEvent(CDM::enumPatientEvent::IrreversibleState, true, m_data.GetSimulationTime());
        } else {
          if (m_PatientActions->GetOverride()->GetOverrideConformance() == CDM::enumOnOff::On) {
            patient.SetEvent(CDM::enumPatientEvent::IrreversibleState, true, m_data.GetSimulationTime());
          }
        }
      }

      // If the O2 tension is below a critical threshold, the irreversible damage occurs more quickly
      if (m_brainO2->GetPartialPressure(PressureUnit::mmHg) < 10.0) {
        /// \event Patient: Critical Brain Oxygen Deficit Event. The oxygen partial pressure in the brain has dropped to a critically low level.
        patient.SetEvent(CDM::enumPatientEvent::CriticalBrainOxygenDeficit, true, m_data.GetSimulationTime());
      } else if (m_brainO2->GetPartialPressure(PressureUnit::mmHg) > 12.0) {
        /// \event Patient: End Brain Oxygen Deficit Event. The oxygen partial pressure has risen above 12 mmHg in the brain. If this occurs when the patient has a critical brain oxygen deficit event, it will reverse the event.
        /// The brain is not in a critical oxygen deficit.
        patient.SetEvent(CDM::enumPatientEvent::CriticalBrainOxygenDeficit, false, m_data.GetSimulationTime());
      }

      // Irreversible damage occurs if the critical deficit has gone on too long
      if (patient.GetEventDuration(CDM::enumPatientEvent::CriticalBrainOxygenDeficit, TimeUnit::s) > 600) {
        m_ss << "Brain Oxygen partial pressure is " << m_brainO2->GetPartialPressure(PressureUnit::mmHg) << " and has been below the critical threshold for " << patient.GetEventDuration(CDM::enumPatientEvent::BrainOxygenDeficit, TimeUnit::s) << " seconds. Damage is irreversible.";
        Warning(m_ss);
        /// \irreversible Brain oxygen pressure has been critically low for more than 10 minutes.
        if (!m_PatientActions->HasOverride()) {
          patient.SetEvent(CDM::enumPatientEvent::IrreversibleState, true, m_data.GetSimulationTime());
        } else {
          if (m_PatientActions->GetOverride()->GetOverrideConformance() == CDM::enumOnOff::On) {
            patient.SetEvent(CDM::enumPatientEvent::IrreversibleState, true, m_data.GetSimulationTime());
          }
        }
      }
    } else if (m_brainO2->GetPartialPressure(PressureUnit::mmHg) > 25.0) {
      /// \event Patient: End Brain Oxygen Deficit Event. The oxygen partial pressure has risen above 25 mmHg in the brain. If this occurs when the patient has a brain oxygen deficit event, it will reverse the event.
      /// The brain is getting oxygen.
      patient.SetEvent(CDM::enumPatientEvent::BrainOxygenDeficit, false, m_data.GetSimulationTime());
      // The critical deficit event is also set to false just in case there is an unrealistically rapid transition in oxygen partial pressure.
      patient.SetEvent(CDM::enumPatientEvent::CriticalBrainOxygenDeficit, false, m_data.GetSimulationTime());
    }

    //Myocardium Oxygen Check
    if (m_myocardiumO2->GetPartialPressure(PressureUnit::mmHg) < 5) {
      /// \event Patient: The heart is not receiving enough oxygen. Coronary arteries should dilate to increase blood flow to the heart.
      patient.SetEvent(CDM::enumPatientEvent::MyocardiumOxygenDeficit, true, m_data.GetSimulationTime());

      if (patient.GetEventDuration(CDM::enumPatientEvent::MyocardiumOxygenDeficit, TimeUnit::s) > 2400) // \cite murry1986preconditioning
      {
        m_ss << "Myocardium oxygen partial pressure is  " << m_myocardiumO2->GetPartialPressure(PressureUnit::mmHg) << " and has been sustained for " << patient.GetEventDuration(CDM::enumPatientEvent::MyocardiumOxygenDeficit, TimeUnit::s) << "patient heart muscle has experienced necrosis and is in an irreversible state.";
        Warning(m_ss);
        /// \irreversible Heart has not been receiving enough oxygen for more than 40 min.
        if (!m_PatientActions->HasOverride()) {
          patient.SetEvent(CDM::enumPatientEvent::IrreversibleState, true, m_data.GetSimulationTime());
        } else {
          if (m_PatientActions->GetOverride()->GetOverrideConformance() == CDM::enumOnOff::On) {
            patient.SetEvent(CDM::enumPatientEvent::IrreversibleState, true, m_data.GetSimulationTime());
          }
        }
      }
    } else if (m_myocardiumO2->GetPartialPressure(PressureUnit::mmHg) > 8) {
      /// \event Patient: End Myocardium Oxygen Event. The heart is now receiving enough oxygen. If this occurs when the patient has a heart oxygen deficit event, it will reverse the event.
      /// The brain is getting oxygen.
      patient.SetEvent(CDM::enumPatientEvent::MyocardiumOxygenDeficit, false, m_data.GetSimulationTime());
    }
  }
}

//--------------------------------------------------------------------------------------------------
/// \brief
/// Sets data on the metabolic panel object.
///
/// \details
/// Sets data on the metabolic panel object to create the metabolic panel.
/// Uses information from the chem 14 substances that are in %BioGears (see @ref bloodchemistry-assessments)
//--------------------------------------------------------------------------------------------------
bool BloodChemistry::CalculateComprehensiveMetabolicPanel(SEComprehensiveMetabolicPanel& cmp)
{
  cmp.Reset();
  cmp.GetAlbumin().Set(m_data.GetSubstances().GetAlbumin().GetBloodConcentration());
  //cmp.GetALP().SetValue();
  //cmp.GetALT().SetValue();
  //cmp.GetAST().SetValue();
  cmp.GetBUN().Set(GetBloodUreaNitrogenConcentration());
  cmp.GetCalcium().Set(m_data.GetSubstances().GetCalcium().GetBloodConcentration());
  double CL_mmol_Per_L = m_data.GetSubstances().GetChloride().GetBloodConcentration(MassPerVolumeUnit::g_Per_L) / m_data.GetSubstances().GetChloride().GetMolarMass(MassPerAmountUnit::g_Per_mmol);
  cmp.GetChloride().SetValue(CL_mmol_Per_L, AmountPerVolumeUnit::mmol_Per_L);
  // CO2 is predominantly Bicarbonate, so going to put that in this slot
  double HCO3_mmol_Per_L = m_data.GetSubstances().GetBicarbonate().GetBloodConcentration(MassPerVolumeUnit::g_Per_L) / m_data.GetSubstances().GetHCO3().GetMolarMass(MassPerAmountUnit::g_Per_mmol);
  cmp.GetCO2().SetValue(HCO3_mmol_Per_L, AmountPerVolumeUnit::mmol_Per_L);
  cmp.GetCreatinine().Set(m_data.GetSubstances().GetCreatinine().GetBloodConcentration());
  cmp.GetGlucose().Set(m_data.GetSubstances().GetGlucose().GetBloodConcentration());
  double K_mmol_Per_L = m_data.GetSubstances().GetPotassium().GetBloodConcentration(MassPerVolumeUnit::g_Per_L) / m_data.GetSubstances().GetPotassium().GetMolarMass(MassPerAmountUnit::g_Per_mmol);
  cmp.GetPotassium().SetValue(K_mmol_Per_L, AmountPerVolumeUnit::mmol_Per_L);
  double Sodium_mmol_Per_L = m_data.GetSubstances().GetSodium().GetBloodConcentration(MassPerVolumeUnit::g_Per_L) / m_data.GetSubstances().GetSodium().GetMolarMass(MassPerAmountUnit::g_Per_mmol);
  cmp.GetSodium().SetValue(Sodium_mmol_Per_L, AmountPerVolumeUnit::mmol_Per_L);
  //cmp.GetTotalBelirubin().SetValue();
  cmp.GetTotalProtein().Set(GetTotalProteinConcentration());
  return true;
}

//--------------------------------------------------------------------------------------------------
/// \brief
/// Sets data on the complete blood count object.
///
/// \details
/// Sets data on the complete blood count object to create the [CBC](@ref bloodchemistry-assessments).
//--------------------------------------------------------------------------------------------------
bool BloodChemistry::CalculateCompleteBloodCount(SECompleteBloodCount& cbc)
{
  cbc.Reset();
  cbc.GetHematocrit().Set(GetHematocrit());
  cbc.GetHemoglobin().Set(m_data.GetSubstances().GetHb().GetBloodConcentration());
  cbc.GetPlateletCount().SetValue(325000, AmountPerVolumeUnit::ct_Per_uL); // Hardcoded for now, don't support PlateletCount yet
  cbc.GetMeanCorpuscularHemoglobin().SetValue(m_data.GetConfiguration().GetMeanCorpuscularHemoglobin(MassPerAmountUnit::pg_Per_ct), MassPerAmountUnit::pg_Per_ct);
  cbc.GetMeanCorpuscularHemoglobinConcentration().SetValue(m_data.GetSubstances().GetHb().GetBloodConcentration(MassPerVolumeUnit::g_Per_dL) / GetHematocrit().GetValue(), MassPerVolumeUnit::g_Per_dL); //Average range should be 32-36 g/dL. (https://en.wikipedia.org/wiki/Mean_corpuscular_hemoglobin_concentration)
  cbc.GetMeanCorpuscularVolume().SetValue(m_data.GetConfiguration().GetMeanCorpuscularVolume(VolumeUnit::uL), VolumeUnit::uL);
  double rbcCount_ct_Per_uL = m_venaCava->GetSubstanceQuantity(m_data.GetSubstances().GetRBC())->GetMolarity(AmountPerVolumeUnit::ct_Per_uL);
  cbc.GetRedBloodCellCount().SetValue(rbcCount_ct_Per_uL, AmountPerVolumeUnit::ct_Per_uL);
  double wbcCount_ct_Per_uL = m_venaCava->GetSubstanceQuantity(m_data.GetSubstances().GetWBC())->GetMolarity(AmountPerVolumeUnit::ct_Per_uL); //m_data.GetSubstances().GetWBC().GetCellCount(AmountPerVolumeUnit::ct_Per_uL);
  cbc.GetWhiteBloodCellCount().SetValue(wbcCount_ct_Per_uL, AmountPerVolumeUnit::ct_Per_uL);
  return true;
}

//--------------------------------------------------------------------------------------------------
/// \brief
/// Reaction when incompatible blood is transfused
///
/// \details
/// Blood reaction causes rbc to become agglutinated/dead and they can no longer bind gases and Hb to transport, thus causing an immune response
//--------------------------------------------------------------------------------------------------
void BloodChemistry::CalculateHemolyticTransfusionReaction(bool rhMismatch)
{
  SEPatient& patient = m_data.GetPatient();
  //patient.SetEvent(CDM::enumPatientEvent::HemolyticTransfusionReaction, true, m_data.GetSimulationTime());

  double dt = m_data.GetTimeStep().GetValue(TimeUnit::s);
  //Calculate ratio of acceptable cells to unacceptable (ex: 0.5 L of transfused B blood compared to a 4.5 L TBV of A blood would be a ratio 1:9
  double patientRBC;
  double donorRBC;
  double TotalRBC;
  double patientAntigen_ct_per_uL;
  double donorAntigen_ct_per_uL;
  double patientAntigen_ct;
  double donorAntigen_ct;
  double BloodVolume = m_data.GetCardiovascular().GetBloodVolume(VolumeUnit::uL);
  const double rbcMass_ug = 2.7e-5;
  const double antigens_per_rbc = 2000000.0;
  const double agglutinin_per_rbc = 50.0;

  double RBC_initial_ct = 0.0;
  double AntA_initial_ct = 0.0;
  double AntB_initial_ct = 0.0;
  SESubstance& RBC_initial = m_data.GetSubstances().GetRBC();
  SESubstance& AntigenA_initial = m_data.GetSubstances().GetAntigen_A();
  SESubstance& AntigenB_initial = m_data.GetSubstances().GetAntigen_B();

  /////////////////////////////////////////////////
  std::vector<SELiquidCompartment*> vascularcompts = m_data.GetCompartments().GetVascularLeafCompartments();
  for (auto v : vascularcompts) {
    SELiquidCompartment* compt = v;
    double ComptVolume = compt->GetVolume().GetValue(VolumeUnit::uL);
    RBC_initial_ct += (compt->GetSubstanceQuantity(RBC_initial)->GetMolarity().GetValue(AmountPerVolumeUnit::ct_Per_uL) * ComptVolume); //.SetValue(compt->GetSubstanceQuantity(Hemoglobin)->GetMass(MassUnit::g) * deadCells_percent, MassUnit::g);
    AntA_initial_ct += (compt->GetSubstanceQuantity(AntigenA_initial)->GetMolarity().GetValue(AmountPerVolumeUnit::ct_Per_uL) * ComptVolume);
    AntB_initial_ct += (compt->GetSubstanceQuantity(AntigenB_initial)->GetMolarity().GetValue(AmountPerVolumeUnit::ct_Per_uL) * ComptVolume);
  }
  //////////////////////////////////////////////

  /*double AntigenA = m_venaCava->GetSubstanceQuantity(m_data.GetSubstances().GetAntigen_A())->GetMolarity(AmountPerVolumeUnit::ct_Per_uL);
  double AntigenB = m_venaCava->GetSubstanceQuantity(m_data.GetSubstances().GetAntigen_B())->GetMolarity(AmountPerVolumeUnit::ct_Per_uL);
  double rbcCount_ct_Per_uL = m_venaCava->GetSubstanceQuantity(m_data.GetSubstances().GetRBC())->GetMolarity(AmountPerVolumeUnit::ct_Per_uL);*/

  /////////////////////////////////////////////////////////////////////////////////
  if (rhMismatch == true) {
    m_RhFactorMismatch = (antigens_per_rbc * (RBC_initial_ct / BloodVolume)) * m_data.GetDrugs().GetTransfusionReactionVolume().GetValue(VolumeUnit::uL); //Calculated to be count of mismatched factors, treated as "donor antigen" after
  }
  /////////////////////////////////////////////////////////////////////////////////

  if (rhMismatch == false) {
    if (m_data.GetPatient().GetBloodType() == (CDM::enumBloodType::O)) {
      /*donorAntigen_ct_per_uL = AntigenA + AntigenB;
      patientAntigen_ct_per_uL = 0.0;*/
      donorAntigen_ct = AntA_initial_ct + AntB_initial_ct;
      patientAntigen_ct = 0.0;
    } else if (m_data.GetPatient().GetBloodType() == (CDM::enumBloodType::A)) {
      /*donorAntigen_ct_per_uL = AntigenB;
      patientAntigen_ct_per_uL = AntigenA;*/
      donorAntigen_ct = AntB_initial_ct;
      patientAntigen_ct = AntA_initial_ct;
    } else if (m_data.GetPatient().GetBloodType() == (CDM::enumBloodType::B)) {
      /*donorAntigen_ct_per_uL = AntigenA;
      patientAntigen_ct_per_uL = AntigenB;*/
      donorAntigen_ct = AntA_initial_ct;
      patientAntigen_ct = AntB_initial_ct;
    }
  } else {
    donorAntigen_ct = m_RhFactorMismatch;
    patientAntigen_ct = 0.0;
  }

  //TotalRBC = rbcCount_ct_Per_uL * BloodVolume;
  //donorRBC = (donorAntigen_ct_per_uL * BloodVolume) / antigens_per_rbc;
  //patientRBC = TotalRBC - donorRBC; // Calculated this way to account for O cells still having no antigens
  TotalRBC = RBC_initial_ct;
  donorRBC = donorAntigen_ct / antigens_per_rbc;
  patientRBC = TotalRBC - donorRBC;

  double HealExp = 1.0 - (patientRBC / (5280000.0 * 4800000.0)); // uL of blood at steady state time ct/uL of rbc
  BLIM(HealExp, 0.0, 1.0);
  double HealFactor = std::pow(2.0, HealExp);
  double RBC_birth = HealFactor * m_data.GetSubstances().GetRBC().GetClearance().GetCellBirthRate().GetValue(FrequencyUnit::Per_s); //per s(increased by factor of 10)
  double RBC_death = m_data.GetSubstances().GetRBC().GetClearance().GetCellDeathRate(FrequencyUnit::Per_s); //per s

  //Surface Area of rbc stacked
  double sa1 = 1.3e-6;
  double sa2 = 1.9e-6;
  double sa3 = 2.4e-6;
  double antpercm2 = antigens_per_rbc / sa1;
  double aggpercm2 = agglutinin_per_rbc / sa1;

  double D1 = 1.4e-11; //cm2 per s and is diffusion coefficient of one rbc
  double R1 = 3.5e-4; //cm and is radius of one rbc
  double R2 = R1 * std::cbrt(2);
  double R3 = R1 * std::cbrt(3);
  double D2 = (D1 * R1) / (R2);
  double D3 = (D1 * R1) / (R3);

  double agg1 = (aggpercm2 * sa1);
  double ant1 = (antpercm2 * sa1);
  double agg2 = (aggpercm2 * sa2);
  double ant2 = (antpercm2 * sa2);
  double agg3 = (aggpercm2 * sa3);
  double ant3 = (antpercm2 * sa3);

  double timedelay = 5e-4; //tuning factor for timing of response, increasing does more agglutination, play with this
  double tuning11 = timedelay / ((agg1 + ant1) * (agg1 + ant1));
  double tuning12 = timedelay / ((agg1 + ant1) * (agg2 + ant2));
  double tuning22 = timedelay / ((agg2 + ant2) * (agg2 + ant2));
  double tuning13 = timedelay / ((agg1 + ant1) * (agg3 + ant3));

  //Agglutinate Probabilities
  double oneK1 = tuning11 * ((2.0 * D1) * (2.0 * R1)) / (4.0 * D1 * R1);
  double twoK1 = tuning12 * ((D1 + D2) * (R1 + R2)) / (4.0 * D1 * R1);
  double twoK2 = tuning22 * ((D2 + D2) * (R2 + R2)) / (4.0 * D1 * R1);
  double threeK1 = tuning13 * ((D3 + D1) * (R3 + R1)) / (4.0 * D1 * R1);

  double newC1RBC_patient = patientRBC + (RBC_birth * dt) - (RBC_death * dt) - (dt * ((oneK1 * patientRBC * donorRBC) + (twoK1 * m_2Agglutinate * patientRBC) + (threeK1 * m_d3Agglutinate * patientRBC)));
  double newC1RBC_donor = donorRBC - (RBC_death * dt) - (dt * ((oneK1 * patientRBC * donorRBC) + (twoK1 * m_2Agglutinate * donorRBC) + (threeK1 * m_p3Agglutinate * donorRBC)));
  double newC2RBC = m_2Agglutinate + (dt * ((oneK1 * patientRBC * donorRBC) - (twoK1 * m_2Agglutinate * patientRBC) - (twoK1 * m_2Agglutinate * donorRBC) - (twoK2 * m_2Agglutinate * m_2Agglutinate)));
  double newC3RBC_patient = m_p3Agglutinate + (dt * ((twoK1 * m_2Agglutinate * patientRBC) - (threeK1 * m_p3Agglutinate * donorRBC)));
  double newC3RBC_donor = m_d3Agglutinate + (dt * ((twoK1 * m_2Agglutinate * donorRBC) - (threeK1 * m_d3Agglutinate * patientRBC)));
  double newC4RBC = m_4Agglutinate + (dt * ((twoK2 * m_2Agglutinate * m_2Agglutinate) + (threeK1 * m_d3Agglutinate * patientRBC) + (threeK1 * m_p3Agglutinate * donorRBC)));

  m_patientRBC = newC1RBC_patient;
  LLIM(m_patientRBC, 0.0);
  m_donorRBC = newC1RBC_donor;
  LLIM(m_donorRBC, 0.0);
  if (m_donorRBC == 0.0) {
    patient.SetEvent(CDM::enumPatientEvent::HemolyticTransfusionReaction, false, m_data.GetSimulationTime());
  }
  m_2Agglutinate = newC2RBC;
  LLIM(m_2Agglutinate, 0.0);
  m_p3Agglutinate = newC3RBC_patient;
  LLIM(m_p3Agglutinate, 0.0);
  m_d3Agglutinate = newC3RBC_donor;
  LLIM(m_d3Agglutinate, 0.0);
  m_4Agglutinate = newC4RBC;
  LLIM(m_4Agglutinate, 0.0);

  ////////////////////////////////////////////////////////////////////////
  double effectTune = 2e-1;
  double newAggglutinates;
  newAggglutinates = (m_4Agglutinate + (m_p3Agglutinate / 12.0)) - m_RemovedRBC; // p3Agglutinates partially removed to scale for controlled donor cells (multiplied by 4 in next step)
  double deadCells_percent = ((effectTune * (newAggglutinates * 4.0)) / TotalRBC);
  BLIM(deadCells_percent, 0.0, 1.0);
  double lostOxygen_percent = deadCells_percent; //  / 5.0; //  *5.0; smaller number here removes less oxygen
  double liveCells_percent = 1.0 - deadCells_percent;
  lostOxygen_percent = 1.0 - lostOxygen_percent;
  double metabolicIncrease = 1.0 + (deadCells_percent / 50.0); // smaller number here increases metabolism
  /*std::stringstream ss;
  ss << m_4Agglutinate << ", " << m_RemovedRBC << ", " << TotalRBC << ", " << lostOxygen_percent << "!!!!!!!!!!!!! ";
  Info(ss);*/
  //SESubstance& RBC_final = m_data.GetSubstances().GetRBC();
  SESubstance& Hemoglobin = m_data.GetSubstances().GetHb();
  SESubstance& HemoglobinO2 = m_data.GetSubstances().GetHbO2();
  SESubstance& HemoglobinCO2 = m_data.GetSubstances().GetHbCO2();
  SESubstance& HemoglobinCO = m_data.GetSubstances().GetHbCO();
  SESubstance& HemoglobinO2CO2 = m_data.GetSubstances().GetHbO2CO2();
  SESubstance& Oxygen = m_data.GetSubstances().GetO2();
  //SESubstance& AntA_final = m_data.GetSubstances().GetAntigen_A();
  //SESubstance& AntB_final = m_data.GetSubstances().GetAntigen_B();

  m_RemovedRBC = m_4Agglutinate + (m_p3Agglutinate / 12.0);
  //double initialHb = GetHemoglobinContent(MassUnit::g)*10000.0;

  std::vector<SELiquidCompartment*> vascularcompts2 = m_data.GetCompartments().GetVascularLeafCompartments();
  for (auto v : vascularcompts2) {
    SELiquidCompartment* compt = v;

    double ComptVolume = compt->GetVolume().GetValue(VolumeUnit::uL);
    compt->GetSubstanceQuantity(Hemoglobin)->GetMass().SetValue(compt->GetSubstanceQuantity(Hemoglobin)->GetMass(MassUnit::g) * liveCells_percent, MassUnit::g);
    compt->GetSubstanceQuantity(HemoglobinO2)->GetMass().SetValue(compt->GetSubstanceQuantity(HemoglobinO2)->GetMass(MassUnit::g) * liveCells_percent, MassUnit::g);
    compt->GetSubstanceQuantity(HemoglobinCO2)->GetMass().SetValue(compt->GetSubstanceQuantity(HemoglobinCO2)->GetMass(MassUnit::g) * liveCells_percent, MassUnit::g);
    compt->GetSubstanceQuantity(HemoglobinO2CO2)->GetMass().SetValue(compt->GetSubstanceQuantity(HemoglobinO2CO2)->GetMass(MassUnit::g) * liveCells_percent, MassUnit::g);
    compt->GetSubstanceQuantity(Oxygen)->GetMass().SetValue(compt->GetSubstanceQuantity(Oxygen)->GetMass(MassUnit::g) * lostOxygen_percent, MassUnit::g);
    compt->GetSubstanceQuantity(Hemoglobin)->Balance(BalanceLiquidBy::Mass);
    compt->GetSubstanceQuantity(HemoglobinO2)->Balance(BalanceLiquidBy::Mass);
    compt->GetSubstanceQuantity(HemoglobinCO2)->Balance(BalanceLiquidBy::Mass);
    compt->GetSubstanceQuantity(HemoglobinO2CO2)->Balance(BalanceLiquidBy::Mass);
    compt->GetSubstanceQuantity(Oxygen)->Balance(BalanceLiquidBy::Mass);

    compt->GetSubstanceQuantity(RBC_initial)->GetMolarity().SetValue(compt->GetSubstanceQuantity(RBC_initial)->GetMolarity().GetValue(AmountPerVolumeUnit::ct_Per_uL) * liveCells_percent, AmountPerVolumeUnit::ct_Per_uL);
    double AntA_Molarity = compt->GetSubstanceQuantity(AntigenA_initial)->GetMolarity().GetValue(AmountPerVolumeUnit::ct_Per_uL);
    double AntB_Molarity = compt->GetSubstanceQuantity(AntigenA_initial)->GetMolarity().GetValue(AmountPerVolumeUnit::ct_Per_uL);
    double AntigensLost = (deadCells_percent * (AntA_Molarity + AntB_Molarity)) / 2.0;
    double AntARemaining_ct_per_uL = compt->GetSubstanceQuantity(AntigenA_initial)->GetMolarity().GetValue(AmountPerVolumeUnit::ct_Per_uL) - AntigensLost;
    double AntBRemaining_ct_per_uL = compt->GetSubstanceQuantity(AntigenB_initial)->GetMolarity().GetValue(AmountPerVolumeUnit::ct_Per_uL) - AntigensLost;
    LLIM(AntARemaining_ct_per_uL, 0.0);
    LLIM(AntBRemaining_ct_per_uL, 0.0);
    compt->GetSubstanceQuantity(AntigenA_initial)->GetMolarity().SetValue(AntARemaining_ct_per_uL, AmountPerVolumeUnit::ct_Per_uL);
    compt->GetSubstanceQuantity(AntigenB_initial)->GetMolarity().SetValue(AntBRemaining_ct_per_uL, AmountPerVolumeUnit::ct_Per_uL);
    compt->GetSubstanceQuantity(RBC_initial)->Balance(BalanceLiquidBy::Molarity);
    compt->GetSubstanceQuantity(AntigenA_initial)->Balance(BalanceLiquidBy::Molarity);
    compt->GetSubstanceQuantity(AntigenB_initial)->Balance(BalanceLiquidBy::Molarity);
  }
  m_data.GetEnergy().GetTotalMetabolicRate().SetValue(m_data.GetEnergy().GetTotalMetabolicRate(PowerUnit::J_Per_s) * metabolicIncrease, PowerUnit::J_Per_s); //core temp increase

  /*double RBC_updated = m_venaCava->GetSubstanceQuantity(RBC_final)->GetMolarity(AmountPerVolumeUnit::ct_Per_uL) - ((newAggglutinates * 4.0) / BloodVolume);
  LLIM(RBC_updated, 0.0);
  m_venaCava->GetSubstanceQuantity(RBC_final)->GetMolarity().SetValue(RBC_updated, AmountPerVolumeUnit::ct_Per_uL);
  if (rhMismatch == false) {
    double AntA_updated = m_venaCava->GetSubstanceQuantity(AntA_final)->GetMolarity(AmountPerVolumeUnit::ct_Per_uL) - ((newAggglutinates * 2.0 * 2000000.0) / BloodVolume);
    double AntB_updated = m_venaCava->GetSubstanceQuantity(AntB_final)->GetMolarity(AmountPerVolumeUnit::ct_Per_uL) - ((newAggglutinates * 2.0 * 2000000.0) / BloodVolume);
    LLIM(AntA_updated, 0.0);
    LLIM(AntB_updated, 0.0);
    m_venaCava->GetSubstanceQuantity(AntA_final)->GetMolarity().SetValue(AntA_updated, AmountPerVolumeUnit::ct_Per_uL);
    m_venaCava->GetSubstanceQuantity(AntB_final)->GetMolarity().SetValue(AntB_updated, AmountPerVolumeUnit::ct_Per_uL);
  } else {
    m_RhFactorMismatch = m_RhFactorMismatch - ((newAggglutinates * 2.0 * 2000000.0) / BloodVolume);
    LLIM(m_RhFactorMismatch, 0.0);
  }*/
  ////////////////////////////////////////////////////////////////////////////////
  if (rhMismatch == true) {
    m_RhFactorMismatch = m_RhFactorMismatch - ((newAggglutinates * 2.0 * 2000000.0) / BloodVolume);
    LLIM(m_RhFactorMismatch, 0.0);
  }
  /////////////////////////////////////////////////////////////////////////////////

  double totalHb_g = m_data.GetSubstances().GetSubstanceMass(m_data.GetSubstances().GetHb(), m_data.GetCompartments().GetVascularLeafCompartments(), MassUnit::g);
  double totalHbO2_g = (m_data.GetSubstances().GetSubstanceMass(m_data.GetSubstances().GetHbO2(), m_data.GetCompartments().GetVascularLeafCompartments(), MassUnit::g) / m_data.GetSubstances().GetHbO2().GetMolarMass(MassPerAmountUnit::g_Per_mol)) * m_data.GetSubstances().GetHb().GetMolarMass(MassPerAmountUnit::g_Per_mol);
  double totalHbCO2_g = (m_data.GetSubstances().GetSubstanceMass(m_data.GetSubstances().GetHbCO2(), m_data.GetCompartments().GetVascularLeafCompartments(), MassUnit::g) / m_data.GetSubstances().GetHbCO2().GetMolarMass(MassPerAmountUnit::g_Per_mol)) * m_data.GetSubstances().GetHb().GetMolarMass(MassPerAmountUnit::g_Per_mol);
  double totalHBO2CO2_g = (m_data.GetSubstances().GetSubstanceMass(m_data.GetSubstances().GetHbO2CO2(), m_data.GetCompartments().GetVascularLeafCompartments(), MassUnit::g) / m_data.GetSubstances().GetHbO2CO2().GetMolarMass(MassPerAmountUnit::g_Per_mol)) * m_data.GetSubstances().GetHb().GetMolarMass(MassPerAmountUnit::g_Per_mol);
  double totalHBCO_g = 0.0;
  if (m_aortaCO != nullptr)
    totalHBCO_g = (m_data.GetSubstances().GetSubstanceMass(m_data.GetSubstances().GetHbCO(), m_data.GetCompartments().GetVascularLeafCompartments(), MassUnit::g) / m_data.GetSubstances().GetHbCO().GetMolarMass(MassPerAmountUnit::g_Per_mol)) * m_data.GetSubstances().GetHb().GetMolarMass(MassPerAmountUnit::g_Per_mol);

  double totalHemoglobinO2Hemoglobin_g = totalHb_g + totalHbO2_g + totalHbCO2_g + totalHBO2CO2_g + totalHBCO_g;
  //double HbLost = (initialHb - (GetHemoglobinContent(MassUnit::g)*10000.0));
  //LLIM(HbLost, 0.0);
  //m_HbLostToUrine += (HbLost);
  if (totalHemoglobinO2Hemoglobin_g < m_HbLostToUrine)
    GetHemoglobinLostToUrine().IncrementValue(m_HbLostToUrine - totalHemoglobinO2Hemoglobin_g, MassUnit::g);
  //GetHemoglobinLostToUrine().SetValue(HbLost, MassUnit::g);
}

/// \brief
/// Simulate effects of systemic pathogen after infection
///
/// \details
/// Helper function for Inflammatory Response.  If the source of inflammation is a pathogen, this
/// function is called to simulate effects of systemic pathogen infiltration that are not currently
/// modeled mechanistically.  At low infection levels, these inflammation markers will remain
/// near their baseline values
//--------------------------------------------------------------------------------------------------
void BloodChemistry::ManageSIRS()
{
  SEThermalCircuitPath* coreCompliance = m_data.GetCircuits().GetInternalTemperatureCircuit().GetPath(BGE::InternalTemperaturePath::InternalCoreToGround);

  //Physiological response
  SESubstance& wbcBaseline = m_data.GetSubstances().GetWBC();
  double tissueIntegrity = m_InflammatoryResponse->GetTissueIntegrity().GetValue();
  double neutrophilActive = m_InflammatoryResponse->GetNeutrophilActive().GetValue();

  //Set pathological effects, starting with updating white blood cell count.  Scaled down to get max levels around 25-30k ct_Per_uL
  double wbcCount_ct_Per_uL = m_venaCava->GetSubstanceQuantity(m_data.GetSubstances().GetWBC())->GetMolarity(AmountPerVolumeUnit::ct_Per_uL); //m_data.GetSubstances().GetWBC().GetCellCount(AmountPerVolumeUnit::ct_Per_uL);
  double wbcAbsolute_ct_Per_uL = wbcCount_ct_Per_uL * (1.0 + neutrophilActive / 0.25);
  m_venaCava->GetSubstanceQuantity(m_data.GetSubstances().GetWBC())->GetMolarity().SetValue(wbcAbsolute_ct_Per_uL, AmountPerVolumeUnit::ct_Per_uL);
  //GetWhiteBloodCellCount().SetValue(wbcAbsolute_ct_Per_uL, AmountPerVolumeUnit::ct_Per_uL);

  //Use the tissue integrity output from the inflammation model to track other Systemic Inflammatory metrics.  These relationships were all
  //empirically determined to time symptom onset (i.e. temperature > 38 degC) with the appropriate stage of sepsis
  double sigmoidInput = 1.0 - tissueIntegrity;

  //Respiration effects--Done in Respiratory system

  //Temperature (fever) effects -- Accounted for by Energy::UpdateHeatResistance, which accounts for altered skin blood flow

  //Bilirubin counts (measure of liver perfusion)
  const double baselineBilirubin_mg_Per_dL = 0.70;
  const double maxBilirubin_mg_Per_dL = 26.0; //Not a physiologal max, but Jones2009Sequential (SOFA score) gives max score when total bilirubin > 12 mg/dL
  const double halfMaxWBC = 0.85; //White blood cell fraction that causes half-max bilirubin concentration.  Set well above 0.5 becuase this is a later sign of shock
  const double shapeParam = 10.0; //Empirically determined to make sure we get above 12 mg/dL (severe liver damage) before wbc maxes out
  double totalBilirubin_mg_Per_dL = GeneralMath::LogisticFunction(maxBilirubin_mg_Per_dL, halfMaxWBC, shapeParam, sigmoidInput) + baselineBilirubin_mg_Per_dL;
  GetTotalBilirubin().SetValue(totalBilirubin_mg_Per_dL, MassPerVolumeUnit::mg_Per_dL);

  double basalTissueEnergyDemand_W = m_Patient->GetBasalMetabolicRate(PowerUnit::W) * 0.8; //Discounting the 20% used by brain
  const double maxDeficitMultiplier = 1.0;
  double energyDeficit_W = basalTissueEnergyDemand_W * maxDeficitMultiplier * std::pow(sigmoidInput, 2.0) / (std::pow(sigmoidInput, 2.0) + 0.25 * 0.25);
  m_data.GetEnergy().GetEnergyDeficit().SetValue(energyDeficit_W, PowerUnit::W);
}

//--------------------------------------------------------------------------------------------------
/// \brief
/// Track inflammation from infections, burns, and hemorrhages
///
/// \details
/// This function uses the Diverse Shock States model of Chow, 2005 to project the inflammatory response
/// induced by various insults. The most important output from this model is Tissue Integrity, which is
/// used to scale resistances on tissue pathways, causing the fluid shift that occurs during sepsis
/// and in the case of large burns.  The original model was calibrated for mice and considered endotoxin
/// (pathogen by-products) to be force for infectious inflammation.  Endotoxin was replaced by an actively
/// growing pathogen population subjected to phagocytosis from local immune response, neutrophils, and
/// macrophages. Some parameters are adjusted depending on the source of inflamamtion to better capture
/// the time scale and magnitude of response.
//--------------------------------------------------------------------------------------------------
void BloodChemistry::InflammatoryResponse()
{
  std::vector<CDM::enumInflammationSource> sources = m_InflammatoryResponse->GetInflammationSources();
  double burnTotalBodySurfaceArea = 0.0;

  if (m_data.GetActions().GetPatientActions().HasInfection()) {
    if (std::find(sources.begin(), sources.end(), CDM::enumInflammationSource::Infection) == sources.end()) {
      double initialPathogen = 0.0;
      switch (m_data.GetActions().GetPatientActions().GetInfection()->GetSeverity()) {
      case CDM::enumInfectionSeverity::Mild:
        initialPathogen = 1.0e6;
        break;
      case CDM::enumInfectionSeverity::Moderate:
        initialPathogen = 5.0e6;
        break;
      case CDM::enumInfectionSeverity::Severe:
        initialPathogen = 1.0e7;
        break;
      default:
        initialPathogen = 1.0e6; //Default to very mild infection
      }

      m_InflammatoryResponse->GetLocalPathogen().SetValue(initialPathogen);
      m_InflammatoryResponse->SetActiveTLR(CDM::enumOnOff::On);
      m_InflammatoryResponse->GetInflammationSources().push_back(CDM::enumInflammationSource::Infection);
    }
  }
  if (m_data.GetActions().GetPatientActions().HasBurnWound()) {
    burnTotalBodySurfaceArea = m_data.GetActions().GetPatientActions().GetBurnWound()->GetTotalBodySurfaceArea().GetValue();
    if (std::find(sources.begin(), sources.end(), CDM::enumInflammationSource::Burn) == sources.end()) {
      m_InflammatoryResponse->GetTrauma().SetValue(burnTotalBodySurfaceArea); //This causes inflammatory mediators (particulalary IL-6) to peak around 4 hrs at levels similar to those induced by pathogen
      m_InflammatoryResponse->GetInflammationSources().push_back(CDM::enumInflammationSource::Burn);
    }
  }

  //Perform this check after looking for inflammatory actions (otherwise we'll never process)
  if (!m_InflammatoryResponse->HasInflammationSources()) {
    return;
  }

  //------------------Previous State--------------------------------------
  double PT = 0.0, MT = 0.0, NT = 0.0, B = 0.0, PB = 0.0, MR = 0.0, MA = 0.0, NR = 0.0, NA = 0.0, ER = 0.0, EA = 0.0, eNOS = 0.0, iNOSd = 0.0, iNOS = 0.0, NO3 = 0.0, NO = 0.0, I6 = 0.0, I10 = 0.0, I12 = 0.0, TNF = 0.0, TI = 0.0, TR = 0.0, R = 0.0;
  PT = m_InflammatoryResponse->GetLocalPathogen().GetValue(); //Local tissue pathogen
  MT = m_InflammatoryResponse->GetLocalMacrophage().GetValue(); //Local tissue macrophages
  NT = m_InflammatoryResponse->GetLocalNeutrophil().GetValue(); //Local tissue neutrophil
  B = m_InflammatoryResponse->GetLocalBarrier().GetValue(); //Local tissue barrier integrity
  CDM::enumOnOff::value TLR = m_InflammatoryResponse->GetActiveTLR(); //Toll-like receptors:  When active, promote degradation of local tissue barrier integrity
  PB = m_InflammatoryResponse->GetBloodPathogen().GetValue(); //Pathogen that has passed through local tissue barrier into bloodstream
  MR = m_InflammatoryResponse->GetMacrophageResting().GetValue(); //Resting blood macrophages
  MA = m_InflammatoryResponse->GetMacrophageActive().GetValue(); //Active blood macrophages
  NR = m_InflammatoryResponse->GetNeutrophilResting().GetValue(); //Resting blood neutrophils
  NA = m_InflammatoryResponse->GetNeutrophilActive().GetValue(); //Active blood neutrophils
  eNOS = m_InflammatoryResponse->GetConstitutiveNOS().GetValue(); //Blood nitrogen oxide synthase (constituitive)
  iNOSd = m_InflammatoryResponse->GetInducibleNOSPre().GetValue(); //Blood nitrogen oxide synthase (pre-inducible)
  iNOS = m_InflammatoryResponse->GetInducibleNOS().GetValue(); //Blood nitrogen oxide syntase (induced)
  NO3 = m_InflammatoryResponse->GetNitrate().GetValue(); //Blood nitrate--product of NO (unstable radical)
  NO = m_InflammatoryResponse->GetNitricOxide().GetValue(); //Blood nitric oxide
  I6 = m_InflammatoryResponse->GetInterleukin6().GetValue(); //Blood interleukin-6
  I10 = m_InflammatoryResponse->GetInterleukin10().GetValue(); //Blood interleukin-10
  I12 = m_InflammatoryResponse->GetInterleukin12().GetValue(); //Blood interleukin-12
  TNF = m_InflammatoryResponse->GetTumorNecrosisFactor().GetValue(); //Blood tumor-necrosis factor
  TI = m_InflammatoryResponse->GetTissueIntegrity().GetValue(); //Global tissue integrity
  TR = m_InflammatoryResponse->GetTrauma().GetValue(); //Trauma

  //------------------------------Model Parameters-----------------------------
  //Time
  double dt_hr = m_data.GetTimeStep().GetValue(TimeUnit::hr);
  double scale = 1.0; //This parameter can be set very high to investigate state equation trajectores (i.e. set to 60 to simulate 30 hrs in 30 min).  Note that there is no guarantee of validity of other BG outputs
  //----Tissue parameters are taken from Dominguez2017Mathematical; kap = growth rate, psi = degradation rate, eps = inhibition, del = decay (other params defined)
  //Tissue pathogen
  double thetaP = 1.35e-4; //Rate of bacteria translocation from tissue to blood
  double epsPB = 3.1, psiPM = 6.3e-3, psiPN = 6.1e-4, kapP = 0.6;
  double uP = 3.7e4; //Saturation constant for bacteria
  //Tissue macrophage
  double Mv = 3.0e-1; //Resting macrophage pool
  double delM = 6.4e-5, epsMB = 3.6e1;
  double beta = 2.6e-2; //Activation of macrophages
  //Tissue neutrophil
  double Nv = 1e8; //Resting neutrophil pool
  double delN = 6.1e-2, epsNB = 3.6e1, epsNM = 1.6e-1;
  double alpha = 6.975e-7; //Activation of neutrophils
  //Local barrier
  double kapB = 4.6e-2, epsBP = 2.6e1, psiBP = 1.4e-1, psiBN = 4.0e-8;
  //TLR switch
  double pUpper = 2.0e6;
  double pLower = 1.0e3;
  //--Blood parameters are from Chow2005Diverse; kYZ = effect of Z on Y, xYZ = amount of Z to bring effect to half its max
  //Blood Source terms
  double sM = 1.0, sN = 1.0, s6 = 0.001, s10 = 0.01;
  //Blood Pathogen parameters
  double kPN = 5.8; //Phagocytic effect of activated neutrophils on pathogen, determined empirically
  double xPN = 0.5; //Level of pathogen that brings elimination of P by neutrophils to 50% of max
  double kPS = 6.9e3; //Background immune response to pathogen in blood
  double xPS = 1.3e4; //Saturation of background immune response
  //Trauma decay
  double kTr = 0.0; //Base value--will be adjusted during burn/hemorrhage (see below)
  //Macrophage interaction
  double kML = 1.01e2, kMTR = 0.04, kM6 = 0.1, kMB = 0.0495, kMR = 0.05, kMD = 0.05, xML = 37.5, xMD = 0.75, xMTNF = 0.4, xM6 = 1.0, xM10 = 0.297, xMCA = 0.9; //Note xMD was 1.0 for burn, see if this messes things up
  //Activate macrophage interactions
  double kMANO = 0.2, kMA = 0.2;
  //Neutrophil interactions -- kN6 and kNTNF tuned for infection
  double kNL = 3.375e1, kNTNF = 0.4, kN6 = 1.5, kNB = 0.1, kND = 0.05, kNTR = 0.02, kNTGF = 0.1, kNR = 0.05, kNNO = 0.4, kNA = 0.5, xNL = 56.25, xNTNF = 2.0, xN6 = 1.0, xND = 0.4, xN10 = 0.2; //xND was 0.4 for burn
  //Inducible nitric oxide synthase
  double kINOSN = 1.5, kINOSM = 0.1, kINOSEC = 0.1, kINOS6 = 2.0, kINOSd = 0.05, kINOS = 0.101, xINOS10 = 0.1, xINOSTNF = 0.05, xINOS6 = 0.1, xINOSNO = 0.3;
  //Constituitive nitric oxide synthase
  double kENOS = 4.0, kENOSEC = 0.05, xENOSTNF = 0.4, xENOSL = 1.015, xENOSTR = 0.1;
  //Nitric oxide
  double kNO3 = 0.46, kNOMA = 2.0;
  //TNF
  double kTNFN = 2.97, kTNFM = 0.1, kTNF = 1.4, xTNF6 = 0.059, xTNF10 = 0.079;
  //IL6
  double k6M = 3.03, k6TNF = 1.0, k62 = 3.4, k6NO = 2.97, k6 = 0.7, k6N = 0.2, x610 = 0.1782, x6TNF = 0.1, x66 = 0.5, x6NO = 0.4, h66 = 1.0;
  //IL10
  double k10MA = 0.1, k10N = 0.1, k10A = 62.87, k10TNF = 1.485, k106 = 0.051, k10 = 0.35, k10R = 0.1, x10TNF = 0.05, x1012 = 0.01, x106 = 0.08;
  //CA
  double kCA = 0.1, kCATR = 0.16;
  //IL12
  double k12M = 0.303, k12 = 0.05, x12TNF = 0.2, x126 = 0.2, x1210 = 0.2525;
  //Damage
  double kD6 = 0.125, kD = 0.15, xD6 = 0.85, xDNO = 0.5, hD6 = 6.0;
  double kDTR = 0.0; //This is a base value that will be adjusted as a function of type and severity of trauma
  double tiMin = 0.2; //Minimum tissue integrity allowed

  //Antibiotic effects
  double antibacterialEffect = m_data.GetDrugs().GetAntibioticActivity().GetValue();

  //------------------Inflammation source specific modifications and/or actions --------------------------------
  if (burnTotalBodySurfaceArea != 0) {
    //Burns inflammation happens on a differnt time scale.  These parameters were tuned for infecton--return to nominal values
    kDTR = 10.0 * burnTotalBodySurfaceArea; //We assume that larger burns inflict damage more rapidly
    kTr = 0.5 / burnTotalBodySurfaceArea; //We assume that larger burns take longer for trauma to resolve
    tiMin = 0.05; //Promotes faster damage accumulation
    kD6 = 0.3, xD6 = 0.25, kD = 0.05, kNTNF = 0.2, kN6 = 0.557, hD6 = 4, h66 = 4.0, x1210 = 0.049;
    scale = 1.0;

    if (PB > ZERO_APPROX) {
      ManageSIRS();
    }

    //---------------------State equations----------------------------------------------
    //Differential containers
    double dPT = 0.0, dMT = 0.0, dNT = 0.0, dBT = 0.0, dPB = 0.0, dMR = 0.0, dMA = 0.0, dNR = 0.0, dNA = 0.0, dER = 0.0, dEA = 0.0, dENOS = 0.0, dINOSd = 0.0, dINOS = 0.0, dNO3 = 0.0, dI6 = 0.0, dI10 = 0.0, dI12 = 0.0, dTNF = 0.0, dTI = 0.0, dTR = 0.0, dB = 0.0;
    //TLR state depends on the last TLR state and the tissue pathogen populaton
    if (PT > pUpper) {
      R = 1.0; //TLR always active if pathogen above max threshold
      TLR = CDM::enumOnOff::On;
    } else if (PT > pLower) {
      if (TLR == CDM::enumOnOff::On) {
        R = 1.0; //If pathogen between min/max threshold, it remains at its previous values
      } else {
        R = 0.0;
      }
    } else {
      R = 0.0; //If pathogen below min threshold, it is always inactive
      TLR = CDM::enumOnOff::Off;
    }
    //Process equations
    dPT = (kapP / uP) * PT * (1.0 - PT) - thetaP * PT / (1.0 + epsPB * B) - psiPN * NT * PT - psiPM * MT * PT;
    if (PT < ZERO_APPROX) {
      //Make sure when we get close to P = 0 that we don't take too big a step and pull a negative P for next iteration
      PT = 0.0;
      dPT = 0.0;
    }
    dMT = beta * NT / (1.0 + epsMB * B) * Mv - delM * MT;
    dNT = alpha * R * Nv / ((1.0 + epsNB * B) * (1.0 + epsNM * MT)) - delN * NT;
    dB = kapB / (1.0 + epsBP * R) * B * (1.0 - B) - psiBP * R * B - psiBN * NT * B;
    dPB = (kapP - antibacterialEffect) * PB + thetaP * PT / (1.0 + epsPB * B) - kPS * PB / (xPS + PB) - kPN * NA * GeneralMath::HillActivation(PB, xPN, 2.0);
    dTR = -kTr * TR; //This is assumed to be the driving force for burn
    dMR = -((kML * GeneralMath::HillActivation(PB, xML, 2.0) + kMD * GeneralMath::HillActivation(1.0 - TI, xMD, 4.0)) * (GeneralMath::HillActivation(TNF, xMTNF, 2.0) + kM6 * GeneralMath::HillActivation(I6, xM6, 2.0)) + kMTR * TR) * MR * GeneralMath::HillInhibition(I10, xM10, 2.0) - kMR * (MR - sM);
    dMA = ((kML * GeneralMath::HillActivation(PB, xML, 2.0) + kMD * GeneralMath::HillActivation(1.0 - TI, xMD, 4.0)) * (GeneralMath::HillActivation(TNF, xMTNF, 2.0) + kM6 * GeneralMath::HillActivation(I6, xM6, 2.0)) + kMTR * TR) * MR * GeneralMath::HillInhibition(I10, xM10, 2.0) - kMA * MA;
    dNR = -(kNL * GeneralMath::HillActivation(PB, xNL, 2.0) + kNTNF * xNTNF * GeneralMath::HillActivation(TNF, xNTNF, 1.0) + kN6 * std::pow(xN6, 2.0) * GeneralMath::HillActivation(I6, xN6, 2.0) + kND * std::pow(xND, 2.0) * GeneralMath::HillActivation(1.0 - TI, xND, 2.0) + kNTR * TR) * GeneralMath::HillInhibition(I10, xN10, 2.0) * NR - kNR * (NR - sN);
    dNA = (kNL * GeneralMath::HillActivation(PB, xNL, 2.0) + kNTNF * xNTNF * GeneralMath::HillActivation(TNF, xNTNF, 1.0) + kN6 * std::pow(xN6, 2.0) * GeneralMath::HillActivation(I6, xN6, 2.0) + kND * std::pow(xND, 2.0) * GeneralMath::HillActivation(1.0 - TI, xND, 2.0) + kNTR * TR) * GeneralMath::HillInhibition(I10, xN10, 2.0) * NR - kNA * NA;
    dINOS = kINOS * (iNOSd - iNOS);
    dINOSd = (kINOSN * NA + kINOSM * MA + kINOSEC * (std::pow(xINOSTNF, 2.0) * GeneralMath::HillActivation(TNF, xINOSTNF, 2.0) + kINOS6 * std::pow(xINOS6, 2.0) * GeneralMath::HillActivation(I6, xINOS6, 2.0))) * GeneralMath::HillInhibition(I10, xINOS10, 2.0) * GeneralMath::HillInhibition(NO, xINOSNO, 4.0) - kINOSd * iNOSd;
    dENOS = kENOSEC * GeneralMath::HillInhibition(TNF, xENOSTNF, 1.0) * GeneralMath::HillInhibition(PB, xENOSL, 1.0) * GeneralMath::HillInhibition(TR, xENOSTR, 4.0) - kENOS * eNOS;
    dNO3 = kNO3 * (NO - NO3);
    dTNF = (kTNFN * NA + kTNFM * MA) * GeneralMath::HillInhibition(I10, xTNF10, 2.0) * GeneralMath::HillInhibition(I6, xTNF6, 3.0) - kTNF * TNF;
    dI6 = (k6N * NA + MA) * (k6M + k6TNF * GeneralMath::HillActivation(TNF, x6TNF, 2.0) + k6NO * GeneralMath::HillActivation(NO, x6NO, 2.0)) * GeneralMath::HillInhibition(I10, x610, 2.0) * GeneralMath::HillInhibition(I6, x66, h66) + k6 * (s6 - I6);
    dI10 = (k10N * NA + MA) * (k10MA + k10TNF * GeneralMath::HillActivation(TNF, x10TNF, 4.0) + k106 * GeneralMath::HillActivation(I6, x106, 4.0)) * ((1 - k10R) * GeneralMath::HillInhibition(I12, x1012, 4.0) + k10R) - k10 * (I10 - s10);
    dI12 = k12M * MA * GeneralMath::HillInhibition(I10, x1210, 2.0) - k12 * I12;
    dTI = kD * (1.0 - TI) * (TI - tiMin) * TI - (TI - tiMin) * (kD6 * GeneralMath::HillActivation(I6, xD6, hD6) + kDTR * TR) * (1.0 / (std::pow(xDNO, 2.0) + std::pow(NO, 2.0)));

    //------------------------Update State-----------------------------------------------
    m_InflammatoryResponse->GetLocalPathogen().IncrementValue(dPT * dt_hr * scale);
    m_InflammatoryResponse->GetLocalMacrophage().IncrementValue(dMT * dt_hr * scale);
    m_InflammatoryResponse->GetLocalNeutrophil().IncrementValue(dNT * dt_hr * scale);
    m_InflammatoryResponse->GetLocalBarrier().IncrementValue(dB * dt_hr * scale);
    m_InflammatoryResponse->GetBloodPathogen().IncrementValue(dPB * dt_hr * scale);
    m_InflammatoryResponse->GetTrauma().IncrementValue(dTR * dt_hr * scale);
    m_InflammatoryResponse->GetMacrophageResting().IncrementValue(dMR * dt_hr * scale);
    m_InflammatoryResponse->GetMacrophageActive().IncrementValue(dMA * dt_hr * scale);
    m_InflammatoryResponse->GetNeutrophilResting().IncrementValue(dNR * dt_hr * scale);
    m_InflammatoryResponse->GetNeutrophilActive().IncrementValue(dNA * dt_hr * scale);
    m_InflammatoryResponse->GetInducibleNOS().IncrementValue(dINOS * dt_hr * scale);
    m_InflammatoryResponse->GetInducibleNOSPre().IncrementValue(dINOSd * dt_hr * scale);
    m_InflammatoryResponse->GetConstitutiveNOS().IncrementValue(dENOS * dt_hr * scale);
    m_InflammatoryResponse->GetNitrate().IncrementValue(dNO3 * dt_hr * scale);
    m_InflammatoryResponse->GetTumorNecrosisFactor().IncrementValue(dTNF * dt_hr * scale);
    m_InflammatoryResponse->GetInterleukin6().IncrementValue(dI6 * dt_hr * scale);
    m_InflammatoryResponse->GetInterleukin10().IncrementValue(dI10 * dt_hr * scale);
    m_InflammatoryResponse->GetInterleukin12().IncrementValue(dI12 * dt_hr * scale);
    m_InflammatoryResponse->GetTissueIntegrity().IncrementValue(dTI * dt_hr * scale);
    NO = iNOS * (1.0 + kNOMA * (m_InflammatoryResponse->GetMacrophageActive().GetValue() + m_InflammatoryResponse->GetNeutrophilActive().GetValue())) + eNOS; //Algebraic relationship, not differential
    m_InflammatoryResponse->GetNitricOxide().SetValue(NO);
    m_InflammatoryResponse->SetActiveTLR(TLR);

    //------------------------Check to see if infection-induced inflammation has resolved sufficient to eliminate action-----------------------
    //Note that even though we remove the infection, we leave the inflammation source active.  This is because we want the inflammation markers
    // to return to normal, baseline values.
    const double bloodPathogenLimit = 1.0e-5; //This is 1e-4 % of approximate max blood pathogen that model can eliminate withoug antibiotics
    const double tissuePathogenLimit = 1.0; //This is 1e-4 % of the initial value for a mild infection
    if ((PT < tissuePathogenLimit) && (PB < bloodPathogenLimit) && (m_data.GetActions().GetPatientActions().HasInfection())) {
      m_data.GetActions().GetPatientActions().RemoveInfection();
    }
    //We could put a check here to see if all inflammation states are back to baseline and then remove it from the sources vector
    //However, I think this would only happen over an extremely long run, and so it does not seem likely to be a major need.
  }
}
  //--------------------------------------------------------------------------------------------------
  /// \brief
  /// determine override requirements from user defined inputs
  ///
  /// \details
  /// User specified override outputs that are specific to the cardiovascular system are implemented here.
  /// If overrides aren't present for this system then this function will not be called during preprocess.
  //--------------------------------------------------------------------------------------------------
  void BloodChemistry::ProcessOverride()
  {
    auto override = m_data.GetActions().GetPatientActions().GetOverride();

#ifdef BIOGEARS_USE_OVERRIDE_CONTROL
    OverrideControlLoop();
#endif

    if (override->HasArterialPHOverride()) {
      GetArterialBloodPH().SetValue(override->GetArterialPHOverride().GetValue());
    }
    if (override->HasVenousPHOverride()) {
      GetVenousBloodPH().SetValue(override->GetVenousPHOverride().GetValue());
    }
    if (override->HasCO2SaturationOverride()) {
      GetCarbonDioxideSaturation().SetValue(override->GetCO2SaturationOverride().GetValue());
    }
    if (override->HasCOSaturationOverride()) {
      GetCarbonMonoxideSaturation().SetValue(override->GetCOSaturationOverride().GetValue());
    }
    if (override->HasO2SaturationOverride()) {
      GetOxygenSaturation().SetValue(override->GetO2SaturationOverride().GetValue());
    }
    if (override->HasPhosphateOverride()) {
      GetPhosphate().SetValue(override->GetPhosphateOverride(AmountPerVolumeUnit::mmol_Per_mL), AmountPerVolumeUnit::mmol_Per_mL);
    }
    /*if (override->HasWBCCountOverride()) {
      GetWhiteBloodCellCount().SetValue(override->GetWBCCountOverride(AmountPerVolumeUnit::ct_Per_uL), AmountPerVolumeUnit::ct_Per_uL);
    } */
    if (override->HasTotalBilirubinOverride()) {
      GetTotalBilirubin().SetValue(override->GetTotalBilirubinOverride(MassPerVolumeUnit::mg_Per_mL), MassPerVolumeUnit::mg_Per_mL);
    }
    if (override->HasCalciumConcentrationOverride()) {
      m_data.GetSubstances().GetCalcium().GetBloodConcentration().SetValue(override->GetCalciumConcentrationOverride(MassPerVolumeUnit::mg_Per_mL), MassPerVolumeUnit::mg_Per_mL);
    }
    if (override->HasGlucoseConcentrationOverride()) {
      m_data.GetSubstances().GetGlucose().GetBloodConcentration().SetValue(override->GetGlucoseConcentrationOverride(MassPerVolumeUnit::mg_Per_mL), MassPerVolumeUnit::mg_Per_mL);
    }
    if (override->HasLactateConcentrationOverride()) {
      m_data.GetSubstances().GetLactate().GetBloodConcentration().SetValue(override->GetLactateConcentrationOverride(MassPerVolumeUnit::mg_Per_mL), MassPerVolumeUnit::mg_Per_mL);
    }
    if (override->HasPotassiumConcentrationOverride()) {
      m_data.GetSubstances().GetPotassium().GetBloodConcentration().SetValue(override->GetPotassiumConcentrationOverride(MassPerVolumeUnit::mg_Per_mL), MassPerVolumeUnit::mg_Per_mL);
    }
    if (override->HasSodiumConcentrationOverride()) {
      m_data.GetSubstances().GetSodium().GetBloodConcentration().SetValue(override->GetSodiumConcentrationOverride(MassPerVolumeUnit::mg_Per_mL), MassPerVolumeUnit::mg_Per_mL);
    }
  }

  //// Can be turned on or off (for debugging purposes) using the Biogears_USE_OVERRIDE_CONTROL external in CMake
  void BloodChemistry::OverrideControlLoop()
  {
    auto override = m_data.GetActions().GetPatientActions().GetOverride();
    constexpr double maxArtPHOverride = 14.0; //Arterial pH
    constexpr double minArtPHOverride = 0.0; //Arterial pH
    constexpr double maxVenPHOverride = 14.0; //Venous pH
    constexpr double minVenPHOverride = 0.0; //Venous pH
    constexpr double maxCO2SaturationOverride = 1.0; //Carbon Dioxide Saturation
    constexpr double minCO2SaturationOverride = 0.0; //Carbon Dioxide Saturation
    constexpr double maxCOSaturationOverride = 1.0; //Carbon Monoxide Saturation
    constexpr double minCOSaturationOverride = 0.0; //Carbon Monoxide Saturation
    constexpr double maxO2SaturationOverride = 1.0; //Oxygen Saturation
    constexpr double minO2SaturationOverride = 0.0; //Oxygen Saturation
    constexpr double maxPhosphateOverride = 1000.0; // mmol/mL
    constexpr double minPhosphateOverride = 0.0; // mmol/mL
    constexpr double maxTotalBilirubinOverride = 500.0; // mg/dL
    constexpr double minTotalBilirubinOverride = 0.0; // mg/dL
    constexpr double maxCalciumConcentrationOverride = 500.0; // mg/dL
    constexpr double minCalciumConcentrationOverride = 0.0; // mg/dL
    constexpr double maxGlucoseConcentrationOverride = 1000.0; // mg/dL
    constexpr double minGlucoseConcentrationOverride = 0.0; // mg/dL
    constexpr double maxLactateConcentrationOverride = 1000.0; // mg/dL
    constexpr double minLactateConcentrationOverride = 0.0; // mg/dL
    constexpr double maxPotassiumConcentrationOverride = 500.0; // mg/mL
    constexpr double minPotassiumConcentrationOverride = 0.0; // mg/mL
    constexpr double maxSodiumConcentrationOverride = 500.0; // mg/mL
    constexpr double minSodiumConcentrationOverride = 0.0; // mg/mL

    double currentArtPHOverride = GetArterialBloodPH().GetValue(); //Current Arterial pH, value gets changed in next check
    double currentVenPHOverride = GetVenousBloodPH().GetValue(); //Current Venous pH, value gets changed in next check
    double currentCO2SaturationOverride = 0.0; //value gets changed in next check
    double currentCOSaturationOverride = 0.0; //value gets changed in next check
    double currentO2SaturationOverride = 0.0; //value gets changed in next check
    double currentPhosphateOverride = 0.0; //value gets changed in next check
    double currentTotalBilirubinOverride = 0.0; //value gets changed in next check
    double currentCalciumConcentrationOverride = 0.0; //value gets changed in next check
    double currentGlucoseConcentrationOverride = 0.0; //value gets changed in next check
    double currentLactateConcentrationOverride = 0.0; //value gets changed in next check
    double currentPotassiumConcentrationOverride = 0.0; //value gets changed in next check
    double currentSodiumConcentrationOverride = 0.0; //value gets changed in next check

    if (override->HasArterialPHOverride()) {
      currentArtPHOverride = override->GetArterialPHOverride().GetValue();
    }
    if (override->HasVenousPHOverride()) {
      currentVenPHOverride = override->GetVenousPHOverride().GetValue();
    }
    if (override->HasCO2SaturationOverride()) {
      currentCO2SaturationOverride = override->GetCO2SaturationOverride().GetValue();
    }
    if (override->HasCOSaturationOverride()) {
      currentCOSaturationOverride = override->GetCOSaturationOverride().GetValue();
    }
    if (override->HasO2SaturationOverride()) {
      currentO2SaturationOverride = override->GetO2SaturationOverride().GetValue();
    }
    if (override->HasPhosphateOverride()) {
      currentPhosphateOverride = override->GetPhosphateOverride(AmountPerVolumeUnit::mmol_Per_mL);
    }
    if (override->HasTotalBilirubinOverride()) {
      currentTotalBilirubinOverride = override->GetTotalBilirubinOverride(MassPerVolumeUnit::mg_Per_mL);
    }
    if (override->HasCalciumConcentrationOverride()) {
      currentCalciumConcentrationOverride = override->GetCalciumConcentrationOverride(MassPerVolumeUnit::mg_Per_mL);
    }
    if (override->HasGlucoseConcentrationOverride()) {
      currentGlucoseConcentrationOverride = override->GetGlucoseConcentrationOverride(MassPerVolumeUnit::mg_Per_mL);
    }
    if (override->HasLactateConcentrationOverride()) {
      currentLactateConcentrationOverride = override->GetLactateConcentrationOverride(MassPerVolumeUnit::mg_Per_mL);
    }
    if (override->HasPotassiumConcentrationOverride()) {
      currentPotassiumConcentrationOverride = override->GetPotassiumConcentrationOverride(MassPerVolumeUnit::mg_Per_mL);
    }
    if (override->HasSodiumConcentrationOverride()) {
      currentSodiumConcentrationOverride = override->GetSodiumConcentrationOverride(MassPerVolumeUnit::mg_Per_mL);
    }

    if ((currentArtPHOverride < minArtPHOverride || currentArtPHOverride > maxArtPHOverride) && (override->GetOverrideConformance() == CDM::enumOnOff::On)) {
      m_ss << "Arterial Blood pH Override (BloodChemistry) set outside of bounds of validated parameter override. BioGears is no longer conformant.";
      Info(m_ss);
      override->SetOverrideConformance(CDM::enumOnOff::Off);
    }
    if ((currentVenPHOverride < minVenPHOverride || currentVenPHOverride > maxVenPHOverride) && (override->GetOverrideConformance() == CDM::enumOnOff::On)) {
      m_ss << "Venous Blood pH (BloodChemistry) Override set outside of bounds of validated parameter override. BioGears is no longer conformant.";
      Info(m_ss);
      override->SetOverrideConformance(CDM::enumOnOff::Off);
    }
    if ((currentCO2SaturationOverride < minCO2SaturationOverride || currentCO2SaturationOverride > maxCO2SaturationOverride) && (override->GetOverrideConformance() == CDM::enumOnOff::On)) {
      m_ss << "CO2 Saturation (BloodChemistry) Override set outside of bounds of validated parameter override. BioGears is no longer conformant.";
      Info(m_ss);
      override->SetOverrideConformance(CDM::enumOnOff::Off);
    }
    if ((currentCOSaturationOverride < minCOSaturationOverride || currentCOSaturationOverride > maxCOSaturationOverride) && (override->GetOverrideConformance() == CDM::enumOnOff::On)) {
      m_ss << "CO Saturation (BloodChemistry) Override set outside of bounds of validated parameter override. BioGears is no longer conformant.";
      Info(m_ss);
      override->SetOverrideConformance(CDM::enumOnOff::Off);
    }
    if ((currentO2SaturationOverride < minO2SaturationOverride || currentO2SaturationOverride > maxO2SaturationOverride) && (override->GetOverrideConformance() == CDM::enumOnOff::On)) {
      m_ss << "Oxygen Saturation (BloodChemistry) Override set outside of bounds of validated parameter override. BioGears is no longer conformant.";
      Info(m_ss);
      override->SetOverrideConformance(CDM::enumOnOff::Off);
    }
    if ((currentPhosphateOverride < minPhosphateOverride || currentPhosphateOverride > maxPhosphateOverride) && (override->GetOverrideConformance() == CDM::enumOnOff::On)) {
      m_ss << "Phosphate (BloodChemistry) Override set outside of bounds of validated parameter override. BioGears is no longer conformant.";
      Info(m_ss);
      override->SetOverrideConformance(CDM::enumOnOff::Off);
    }
    if ((currentTotalBilirubinOverride < minTotalBilirubinOverride || currentTotalBilirubinOverride > maxTotalBilirubinOverride) && (override->GetOverrideConformance() == CDM::enumOnOff::On)) {
      m_ss << "Total Bilirubin (BloodChemistry) Override set outside of bounds of validated parameter override. BioGears is no longer conformant.";
      Info(m_ss);
      override->SetOverrideConformance(CDM::enumOnOff::Off);
    }
    if ((currentCalciumConcentrationOverride < minCalciumConcentrationOverride || currentCalciumConcentrationOverride > maxCalciumConcentrationOverride) && (override->GetOverrideConformance() == CDM::enumOnOff::On)) {
      m_ss << "Calcium Concentration (BloodChemistry) Override set outside of bounds of validated parameter override. BioGears is no longer conformant.";
      Info(m_ss);
      override->SetOverrideConformance(CDM::enumOnOff::Off);
    }
    if ((currentGlucoseConcentrationOverride < minGlucoseConcentrationOverride || currentGlucoseConcentrationOverride > maxGlucoseConcentrationOverride) && (override->GetOverrideConformance() == CDM::enumOnOff::On)) {
      m_ss << "Glucose Concentration (BloodChemistry) Override set outside of bounds of validated parameter override. BioGears is no longer conformant.";
      Info(m_ss);
      override->SetOverrideConformance(CDM::enumOnOff::Off);
    }
    if ((currentLactateConcentrationOverride < minLactateConcentrationOverride || currentLactateConcentrationOverride > maxLactateConcentrationOverride) && (override->GetOverrideConformance() == CDM::enumOnOff::On)) {
      m_ss << "Lactate Concentration (BloodChemistry) Override set outside of bounds of validated parameter override. BioGears is no longer conformant.";
      Info(m_ss);
      override->SetOverrideConformance(CDM::enumOnOff::Off);
    }
    if ((currentPotassiumConcentrationOverride < minPotassiumConcentrationOverride || currentPotassiumConcentrationOverride > maxPotassiumConcentrationOverride) && (override->GetOverrideConformance() == CDM::enumOnOff::On)) {
      m_ss << "Potassium Concentration (BloodChemistry) Override set outside of bounds of validated parameter override. BioGears is no longer conformant.";
      Info(m_ss);
      override->SetOverrideConformance(CDM::enumOnOff::Off);
    }
    if ((currentSodiumConcentrationOverride < minSodiumConcentrationOverride || currentSodiumConcentrationOverride > maxSodiumConcentrationOverride) && (override->GetOverrideConformance() == CDM::enumOnOff::On)) {
      m_ss << "Sodium Concentration (BloodChemistry) Override set outside of bounds of validated parameter override. BioGears is no longer conformant.";
      Info(m_ss);
      override->SetOverrideConformance(CDM::enumOnOff::Off);
    }
  }
}

