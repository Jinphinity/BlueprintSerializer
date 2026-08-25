from __future__ import annotations

from pathlib import Path
import unittest


PLUGIN_ROOT = Path(__file__).resolve().parents[2]
SOURCE_PATH = (
    PLUGIN_ROOT
    / "Source"
    / "BlueprintSerializer"
    / "Private"
    / "BlueprintExtractorCommands.cpp"
)
HEADER_PATH = (
    PLUGIN_ROOT
    / "Source"
    / "BlueprintSerializer"
    / "Public"
    / "BlueprintExtractorCommands.h"
)
ANALYZER_SOURCE_PATH = (
    PLUGIN_ROOT
    / "Source"
    / "BlueprintSerializer"
    / "Private"
    / "BlueprintAnalyzer.cpp"
)
ANALYZER_HEADER_PATH = (
    PLUGIN_ROOT
    / "Source"
    / "BlueprintSerializer"
    / "Public"
    / "BlueprintAnalyzer.h"
)


class ManifestBatchSourceContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.source = SOURCE_PATH.read_text(encoding="utf-8")
        cls.header = HEADER_PATH.read_text(encoding="utf-8")
        cls.analyzer_source = ANALYZER_SOURCE_PATH.read_text(encoding="utf-8")
        cls.analyzer_header = ANALYZER_HEADER_PATH.read_text(encoding="utf-8")
        start = cls.source.index(
            "void FBlueprintExtractorCommands::ExportBlueprintsFromManifest"
        )
        end = cls.source.index(
            "void FBlueprintExtractorCommands::AuditAnimationCurves", start
        )
        cls.body = cls.source[start:end]

    def test_console_command_and_callback_are_registered(self) -> None:
        self.assertIn('TEXT("BP_SLZR.ExportBlueprintsFromManifest")', self.source)
        self.assertIn(
            "FConsoleCommandWithArgsDelegate::CreateStatic("
            "&FBlueprintExtractorCommands::ExportBlueprintsFromManifest)",
            self.source,
        )
        self.assertIn(
            "static void ExportBlueprintsFromManifest(const TArray<FString>& Args);",
            self.header,
        )

    def test_dedupe_is_first_occurrence_and_order_preserving(self) -> None:
        duplicate_check = self.body.index("FirstOccurrenceLineByPath.Find(CleanPath)")
        duplicate_continue = self.body.index("continue;", duplicate_check)
        remember_first = self.body.index(
            "FirstOccurrenceLineByPath.Add(CleanPath, LineNumber)"
        )
        append_ordered = self.body.index("OrderedPaths.Add(CleanPath)")
        self.assertLess(duplicate_check, duplicate_continue)
        self.assertLess(duplicate_continue, remember_first)
        self.assertLess(remember_first, append_ordered)
        self.assertIn(
            'TEXT("first_occurrence_exact_case_sensitive_after_trim")', self.body
        )
        self.assertIn('TEXT("requestedPathCount")', self.body)
        self.assertIn('TEXT("uniquePathCount")', self.body)
        self.assertIn('TEXT("duplicateCount")', self.body)

    def test_each_unique_path_gets_a_machine_readable_result(self) -> None:
        self.assertIn("for (int32 ResultIndex = 0; ResultIndex < OrderedPaths.Num();", self.body)
        self.assertIn('TEXT("blueprintPath")', self.body)
        self.assertIn('TEXT("firstOccurrenceLine")', self.body)
        self.assertIn('TEXT("success")', self.body)
        self.assertIn('TEXT("status")', self.body)
        self.assertIn('TEXT("load_failed")', self.body)
        self.assertIn('TEXT("export_failed")', self.body)
        self.assertIn('TEXT("exported")', self.body)

    def test_ordinary_failures_continue_and_final_manifest_is_utf8_json(self) -> None:
        load_failure = self.body.index('TEXT("load_failed")')
        self.assertIn("continue;", self.body[load_failure:])
        self.assertIn(
            "UBlueprintAnalyzer::ExportSingleBlueprintToJSONWithResult(CleanPath, ExportDir)",
            self.body,
        )
        self.assertIn('TEXT("continue_ordinary_object_failures")', self.body)
        self.assertIn('TEXT("BP_SLZR_BatchManifest.json")', self.body)
        self.assertIn(
            "FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM", self.body
        )
        for field in (
            "schemaVersion",
            "sourceManifestPath",
            "requestedPathCount",
            "uniquePathCount",
            "duplicateCount",
            "resultCount",
            "nextResultIndex",
            "successCount",
            "failCount",
            "uniqueRequestedPaths",
            "duplicates",
            "results",
        ):
            self.assertIn(f'TEXT("{field}")', self.body)

    def test_empty_manifest_fails_closed_and_status_distinguishes_completion(self) -> None:
        self.assertIn("if (OrderedPaths.Num() == 0)", self.body)
        self.assertIn('TEXT("source_manifest_contains_no_paths")', self.body)
        self.assertIn('TEXT("overallStatus")', self.body)
        self.assertIn('TEXT("allSucceeded")', self.body)
        self.assertIn('TEXT("completed_with_failures")', self.body)

    def test_results_are_checkpointed_before_the_final_manifest(self) -> None:
        self.assertIn(
            'TEXT("overwrite_fixed_manifest_after_each_object_result")', self.body
        )
        checkpoint_body = self.body.index("auto CheckpointResults = [&]()")
        result_append = self.body.index(
            "Results.Add(MakeShareable(new FJsonValueObject(Result)))", checkpoint_body
        )
        checkpoint_call = self.body.index("CheckpointResults();", result_append)
        final_complete = self.body.index(
            'BatchManifest->SetBoolField(TEXT("complete"), true)', checkpoint_call
        )
        self.assertLess(result_append, checkpoint_call)
        self.assertLess(checkpoint_call, final_complete)

    def test_argument_and_text_format_contracts_are_explicit(self) -> None:
        self.assertIn("Args.Num() == 0 || Args.Num() > 2", self.body)
        self.assertIn('TEXT("unreal-autodetected-text-lines-v1")', self.body)
        self.assertIn('TEXT("commentSyntax")', self.body)
        self.assertIn('TEXT("argumentContract")', self.body)
        self.assertIn(
            "manifest_and_output_paths_must_not_contain_whitespace", self.body
        )
        self.assertNotIn("quote_each_argument_containing_spaces", self.body)

    def test_final_summary_fails_when_result_manifest_cannot_be_saved(self) -> None:
        self.assertIn(
            "bAllObjectAndCheckpointWorkSucceeded && bManifestSaved", self.body
        )
        self.assertIn('TEXT("result_manifest_write_failed")', self.body)
        self.assertIn("checkpointWriteFailures=%d", self.body)

    def test_source_and_exported_json_identities_are_bound(self) -> None:
        for field in (
            "identityAlgorithm",
            "outputIdentityPolicy",
            "sourceManifestIdentityCaptured",
            "sourceManifestBytes",
            "sourceManifestSha256",
            "exportFileSaved",
            "outputIdentityCaptured",
            "outputJsonPath",
            "outputJsonBytes",
            "outputJsonSha256",
            "failureStage",
        ):
            self.assertIn(f'TEXT("{field}")', self.body)

        capture = self.body.index("CaptureFileSha256Identity(")
        decode = self.body.index("FFileHelper::BufferToString(", capture)
        parse = self.body.index("SourceManifestText.ParseIntoArrayLines", decode)
        self.assertLess(capture, decode)
        self.assertLess(decode, parse)
        self.assertIn("&ExactSourceManifestBytes", self.body[capture:decode])
        self.assertNotIn("LoadFileToString(SourceManifestText", self.body)
        self.assertIn(
            "ExportResult.bFileSaved && ExportResult.bOutputIdentityCaptured",
            self.body,
        )
        self.assertIn('TEXT("output_identity_failed")', self.body)

    def test_exporter_result_api_preserves_legacy_bool_callers(self) -> None:
        for declaration in (
            "struct BLUEPRINTSERIALIZER_API FBS_BlueprintFileExportResult",
            "static FBS_BlueprintFileExportResult ExportSingleBlueprintToJSONWithResult(",
            "static bool CaptureFileSha256Identity(",
        ):
            self.assertIn(declaration, self.analyzer_header)
        for field in (
            "bFileSaved",
            "bOutputIdentityCaptured",
            "OutputFilePath",
            "OutputFileBytes",
            "OutputFileSha256",
            "FailureStage",
        ):
            self.assertIn(field, self.analyzer_header)

        legacy_start = self.analyzer_source.index(
            "bool UBlueprintAnalyzer::ExportSingleBlueprintToJSON("
        )
        result_start = self.analyzer_source.index(
            "FBS_BlueprintFileExportResult "
            "UBlueprintAnalyzer::ExportSingleBlueprintToJSONWithResult(",
            legacy_start,
        )
        legacy_body = self.analyzer_source[legacy_start:result_start]
        self.assertIn(
            "return ExportSingleBlueprintToJSONWithResult("
            "BlueprintPath, OutputDirectory).bFileSaved;",
            legacy_body,
        )
        result_body = self.analyzer_source[result_start:]
        self.assertIn("Result.OutputFilePath = FullFilePath", result_body)
        self.assertIn("Result.OutputFileBytes", result_body)
        self.assertIn("Result.OutputFileSha256", result_body)
        self.assertIn("CaptureFileSha256Identity(", result_body)

    def test_sha256_is_plugin_local_and_reads_exact_bytes(self) -> None:
        self.assertIn("FString ComputeSha256Hex(const TArray64<uint8>& Bytes)", self.analyzer_source)
        self.assertIn("static constexpr uint32 RoundConstants[64]", self.analyzer_source)
        self.assertIn("0x428a2f98U", self.analyzer_source)
        self.assertIn("0xc67178f2U", self.analyzer_source)
        self.assertIn("0x6a09e667U", self.analyzer_source)
        self.assertIn("0x5be0cd19U", self.analyzer_source)
        self.assertIn("Tail[TailBytes] = 0x80U", self.analyzer_source)
        self.assertIn("static_cast<uint64>(Bytes.Num()) * 8ULL", self.analyzer_source)
        self.assertIn('TEXT("%08x")', self.analyzer_source)

        identity_start = self.analyzer_source.index(
            "bool UBlueprintAnalyzer::CaptureFileSha256Identity("
        )
        identity_end = self.analyzer_source.index(
            "bool UBlueprintAnalyzer::ExportSingleBlueprintToJSON(", identity_start
        )
        identity_body = self.analyzer_source[identity_start:identity_end]
        self.assertIn("TArray64<uint8> ExactFileBytes", identity_body)
        self.assertIn("FFileHelper::LoadFileToArray(ExactFileBytes", identity_body)
        self.assertIn("OutFileBytes = ExactFileBytes.Num()", identity_body)
        self.assertIn("OutFileSha256 = ComputeSha256Hex(ExactFileBytes)", identity_body)

    def test_timestamped_outputs_make_no_byte_determinism_claim(self) -> None:
        self.assertIn('TEXT("byteDeterministic"), false', self.body)
        self.assertIn('TEXT("byteDeterminismClaim"), TEXT("none")', self.body)
        self.assertIn('TEXT("timestampPolicy")', self.body)
        self.assertIn("completedAtUtc", self.body)
        self.assertIn("do not claim reproducible artifact bytes", self.body)


if __name__ == "__main__":
    unittest.main()
