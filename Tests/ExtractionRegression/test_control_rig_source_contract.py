from __future__ import annotations

from pathlib import Path
import unittest


PLUGIN_ROOT = Path(__file__).resolve().parents[2]
ANALYZER_SOURCE_PATH = (
    PLUGIN_ROOT
    / "Source"
    / "BlueprintSerializer"
    / "Private"
    / "BlueprintAnalyzer.cpp"
)


class ControlRigSourceContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.source = ANALYZER_SOURCE_PATH.read_text(encoding="utf-8")

    def test_ue58_legacy_header_enables_typed_control_rig_extraction(self) -> None:
        include_block_start = self.source.index(
            '#if __has_include("ControlRigBlueprint.h")'
        )
        include_block_end = self.source.index(
            "#if UEARATAME_HAS_CONTROL_RIG", include_block_start + 1
        )
        include_block = self.source[include_block_start:include_block_end]
        self.assertIn('#include "ControlRigBlueprint.h"', include_block)
        self.assertIn('#elif __has_include("ControlRigBlueprintLegacy.h")', include_block)
        self.assertIn('#include "ControlRigBlueprintLegacy.h"', include_block)
        self.assertEqual(include_block.count("#define UEARATAME_HAS_CONTROL_RIG 1"), 2)

    def test_explicit_node_target_is_a_first_class_candidate_and_metadata_field(self) -> None:
        collect_call = self.source.index(
            "CollectExplicitControlRigNodePaths(OutData, CandidateAssetPaths);"
        )
        extraction_call = self.source.index(
            "ExtractControlRigData(CandidatePathArray, OutData);", collect_call
        )
        self.assertLess(collect_call, extraction_call)
        self.assertIn('TEXT("AnimGraphNode_ControlRig")', self.source)
        self.assertIn('TEXT("meta.controlRigAssetPath")', self.source)
        self.assertIn('TEXT("ControlRigAssetReference")', self.source)

    def test_control_rig_dependency_classification_is_type_backed(self) -> None:
        self.assertIn(
            "UControlRigBlueprint* ResolveControlRigBlueprint(UObject* Asset)",
            self.source,
        )
        self.assertIn(
            "UControlRigBlueprint* RigBlueprint = ResolveControlRigBlueprint(Asset);",
            self.source,
        )
        self.assertEqual(self.source.count("Closure.ControlRigs.Add("), 1)
        self.assertNotIn(
            'Candidate.Contains(TEXT("ControlRig")) || '
            'Candidate.Contains(TEXT("/ControlRig/"))',
            self.source,
        )

    def test_legacy_rigvm_client_access_selects_the_concrete_base(self) -> None:
        self.assertIn(
            "static_cast<URigVMBlueprint*>(RigBlueprint)->GetRigVMClient()",
            self.source,
        )

    def test_summary_closure_and_coverage_share_one_rig_record_authority(self) -> None:
        self.assertIn(
            'Rig.RigProperties.Find(TEXT("rigPath"))', self.source
        )
        self.assertIn("Closure.ControlRigs.Add(CanonicalRigPath);", self.source)
        self.assertIn('TEXT("rigPath")', self.source)
        self.assertIn(
            "Coverage.TotalControlRigs = OutData.ControlRigs.Num();", self.source
        )

        extraction_call = self.source.index(
            "ExtractControlRigData(CandidatePathArray, OutData);"
        )
        coverage_call = self.source.index(
            "UpdateExtractionCoverage(OutData.AnimGraph, OutData);", extraction_call
        )
        self.assertLess(extraction_call, coverage_call)


if __name__ == "__main__":
    unittest.main()
