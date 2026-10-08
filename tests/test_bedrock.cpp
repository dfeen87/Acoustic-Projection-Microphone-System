#include <gtest/gtest.h>
#include "apm/apm_core.hpp"
#include "apm/apm_system.h"
#include "apm/smartglasses_apm.hpp"
#include <fstream>
#include <limits>
#include <cstdio>
#include <filesystem>
#include "apm/comms/ptt_controller.hpp"
#include "apm/comms/call_signaling.hpp"
#include "apm/io/audio_device.h"
#include "apm/config.h"
#ifdef HAVE_LOCAL_TRANSLATION
#include "apm/translation/local_translation_engine.hpp"
#endif

TEST(BedrockCore, FailedReinitializationPreservesValidState) {
    apm::APMCore core, reference;
    ASSERT_TRUE(core.initialize(16000, 1));
    ASSERT_TRUE(reference.initialize(16000, 1));
    core.process({0.4f, 0.5f});
    reference.process({0.4f, 0.5f});
    EXPECT_FALSE(core.initialize(0, 2));
    EXPECT_TRUE(core.is_initialized());
    EXPECT_EQ(core.get_sample_rate(), 16000);
    EXPECT_EQ(core.get_num_channels(), 1);
    EXPECT_EQ(core.process({0.1f}), reference.process({0.1f}));
}

TEST(BedrockCore, ExtremeFiniteSamplesDoNotPoisonFilter) {
    apm::APMCore core;
    ASSERT_TRUE(core.initialize());
    const float huge = std::numeric_limits<float>::max();
    auto out = core.process({huge, -huge, huge, 0.1f});
    for (float sample : out) EXPECT_TRUE(std::isfinite(sample));
    for (float sample : core.process({0.1f, 0.0f})) EXPECT_TRUE(std::isfinite(sample));
}

TEST(BedrockCore, TranslationRequiresInitialization) {
    apm::APMCore core;
    auto result = core.translate_text("hello");
    EXPECT_FALSE(result.success);
    EXPECT_FALSE(result.error_message.empty());
}

TEST(BedrockPtt, ShutdownStopsTransmissionBeforeReinitialization) {
    apm::PTTController ptt;
    ptt.set_beep_enabled(false);
    ASSERT_TRUE(ptt.initialize());
    ptt.press();
    ptt.process_audio({0.1f});
    ASSERT_TRUE(ptt.is_transmitting());
    ptt.shutdown();
    EXPECT_FALSE(ptt.is_transmitting());
    auto buffer = ptt.get_audio_buffer();
    ptt.process_audio({0.2f});
    EXPECT_EQ(ptt.get_audio_buffer(), buffer);
    ASSERT_TRUE(ptt.initialize());
    EXPECT_EQ(ptt.get_state(), apm::PTTController::State::IDLE);
}

TEST(BedrockSignaling, StateCallbackCanReadCommittedSession) {
    apm::signaling::CallSignaling signaling;
    signaling.set_heartbeat_interval(1);
    apm::signaling::Participant local{}, remote{};
    local.id = "local"; local.ip_address = "127.0.0.1";
    remote.id = "remote"; remote.ip_address = "127.0.0.1"; remote.port = 9;
    ASSERT_TRUE(signaling.initialize(local, 0));
    signaling.on_call_state_changed([&](const std::string& id, apm::signaling::CallState state) {
        auto sessions = signaling.get_all_sessions();
        ASSERT_FALSE(sessions.empty());
        EXPECT_EQ(sessions.front().session_id, id);
        EXPECT_EQ(sessions.front().state, state);
        if (state == apm::signaling::CallState::ENDED) EXPECT_FALSE(signaling.is_in_call());
    });
    auto id = signaling.initiate_call(remote);
    ASSERT_FALSE(id.empty());
    auto snapshot = signaling.get_session(id);
    ASSERT_TRUE(snapshot.has_value());
    EXPECT_TRUE(signaling.end_call(id));
    EXPECT_EQ(snapshot->state, apm::signaling::CallState::CALLING);
}

TEST(BedrockSignaling, RejectionCannotRewriteAnEndedOutgoingCall) {
    apm::signaling::CallSignaling signaling;
    signaling.set_heartbeat_interval(1);
    apm::signaling::Participant local{}, remote{};
    local.id = "local"; local.ip_address = "127.0.0.1";
    remote.id = "remote"; remote.ip_address = "127.0.0.1"; remote.port = 9;
    ASSERT_TRUE(signaling.initialize(local, 0));
    const auto id = signaling.initiate_call(remote);
    ASSERT_FALSE(id.empty());
    ASSERT_TRUE(signaling.end_call(id));
    const auto ended = signaling.get_session(id);
    ASSERT_TRUE(ended.has_value());
    EXPECT_FALSE(signaling.reject_call(id));
    const auto after = signaling.get_session(id);
    ASSERT_TRUE(after.has_value());
    EXPECT_EQ(after->state, apm::signaling::CallState::ENDED);
    EXPECT_EQ(after->end_time, ended->end_time);
    EXPECT_FALSE(signaling.is_in_call());
}

TEST(BedrockAudio, InvalidConfigurationCannotInitializeHardware) {
    apm::AudioDevice::Config config;
    config.sample_rate = 0;
    EXPECT_THROW(apm::AudioDevice device(config), std::invalid_argument);
}

#ifndef HAVE_PORTAUDIO
TEST(BedrockAudio, MissingDriverCannotClaimActiveCapture) {
    apm::AudioDevice device(apm::AudioDevice::Config{});
    EXPECT_FALSE(device.is_active());
    EXPECT_FALSE(device.start());
    EXPECT_FALSE(device.is_active());
    EXPECT_TRUE(device.stop());
}
#endif

TEST(BedrockFrame, InvalidDimensionsAndChannelsAreRejected) {
    EXPECT_THROW(apm::AudioFrame(10, 0, 1), std::invalid_argument);
    EXPECT_THROW(apm::AudioFrame(10, 16000, -1), std::invalid_argument);
    EXPECT_THROW(apm::AudioFrame(std::numeric_limits<size_t>::max(), 16000, 2), std::length_error);
    apm::AudioFrame frame(3, 16000, 1);
    // Negative channel must be rejected before any interleaved indexing.
    EXPECT_TRUE(frame.channel(-1).empty());
}

TEST(BedrockPipeline, InvalidConfigurationCannotActivate) {
    apm::APMSystem::Config config;
    config.sample_rate = 0;
    EXPECT_THROW(apm::APMSystem system(config), std::invalid_argument);
    EXPECT_THROW(apm::BeamformingEngine(0, 0.01f), std::invalid_argument);
}

TEST(BedrockPipeline, InvalidFrameCannotMutateMonitoring) {
    apm::APMSystem::Config config;
    apm::APMSystem system(config);
    const auto before = system.get_monitoring_metrics();
    std::vector<apm::AudioFrame> microphones(config.num_microphones, apm::AudioFrame(160, config.sample_rate, 1));
    microphones[0].samples()[0] = std::numeric_limits<float>::quiet_NaN();
    apm::AudioFrame reference(160, config.sample_rate, 1);
    EXPECT_TRUE(system.process(microphones, reference, 0.0f).empty());
    EXPECT_FLOAT_EQ(system.get_monitoring_metrics().rms_db, before.rms_db);
    EXPECT_TRUE(system.process(microphones, reference, std::numeric_limits<float>::infinity()).empty());
}

TEST(BedrockProjection, ExtremeFiniteSpacingDoesNotOverflowDelayConversion) {
    apm::DirectionalProjector projector(3, std::numeric_limits<float>::max());
    apm::AudioFrame source(8, 16000, 1);
    std::fill(source.samples().begin(), source.samples().end(), 0.25f);
    const auto output = projector.create_projection_signals(source, 1.0f, 1.5f);
    ASSERT_EQ(output.size(), 3u);
    for (const auto& frame : output) {
        EXPECT_EQ(frame.frame_count(), source.frame_count());
        for (const auto sample : frame.samples()) EXPECT_TRUE(std::isfinite(sample));
    }
}

TEST(BedrockProjection, InvalidSpeakerGeometryCannotActivate) {
    EXPECT_THROW(apm::DirectionalProjector(0, 0.04f), std::invalid_argument);
    EXPECT_THROW(apm::DirectionalProjector(-1, 0.04f), std::invalid_argument);
    for (const auto spacing : {-0.01f, std::numeric_limits<float>::quiet_NaN(),
                               std::numeric_limits<float>::infinity()}) {
        EXPECT_THROW(apm::DirectionalProjector(2, spacing), std::invalid_argument);
    }
    // A colocated array is supported, and finite extreme spacing remains safe.
    EXPECT_NO_THROW(apm::DirectionalProjector(2, 0.0f));
    EXPECT_NO_THROW(apm::DirectionalProjector(2, std::numeric_limits<float>::max()));
}

TEST(BedrockProjection, InvalidDirectionOrDistanceCannotProduceAudio) {
    apm::DirectionalProjector projector(2, 0.04f);
    apm::AudioFrame source(8, 16000, 1);
    std::fill(source.samples().begin(), source.samples().end(), 0.25f);
    EXPECT_TRUE(projector.create_projection_signals(source,
        std::numeric_limits<float>::quiet_NaN(), 1.5f).empty());
    EXPECT_TRUE(projector.create_projection_signals(source, 0.0f,
        std::numeric_limits<float>::infinity()).empty());
    EXPECT_TRUE(projector.create_projection_signals(source, 0.0f, -1.0f).empty());
}

TEST(BedrockSmartglasses, ConfigurationSanitizedBeforeAllocation) {
    apm::SmartglassesApmConfig config;
    config.fft_size = -1;
    EXPECT_NO_THROW(apm::SmartglassesAPM engine(config));
}

TEST(BedrockSmartglasses, OutputMayAliasInputWithoutDestroyingSpeech) {
    apm::SmartglassesAPM reference, engine;
    std::vector<float> left(160, 0.3f), right(160, 0.3f), expected(160);
    apm::SmartglassesFrameMetadata first, second;
    ASSERT_TRUE(reference.process_frame(left, right, expected, first));
    ASSERT_TRUE(engine.process_frame(left, right, left, second));
    EXPECT_EQ(left, expected);
    EXPECT_FLOAT_EQ(first.speech_confidence, second.speech_confidence);
}

TEST(BedrockSmartglasses, FiniteConfigurationAndConsistentDimensions) {
    for (int fft_size : {0, 1, 2, 64, 255, 256}) {
        apm::SmartglassesApmConfig config;
        config.fft_size = fft_size;
        config.alpha_smooth = std::numeric_limits<float>::quiet_NaN();
        config.spatial_exp = std::numeric_limits<float>::infinity();
        apm::SmartglassesAPM engine(config);
        const auto& actual = engine.get_config();
        ASSERT_GE(actual.fft_size, 2);
        ASSERT_LE(actual.frame_size, actual.fft_size);
        ASSERT_TRUE(std::isfinite(actual.alpha_smooth));
        ASSERT_TRUE(std::isfinite(actual.spatial_exp));
        std::vector<float> input(actual.frame_size, 0.2f), output(actual.frame_size);
        apm::SmartglassesFrameMetadata metadata;
        ASSERT_TRUE(engine.process_frame(input, input, output, metadata));
        EXPECT_TRUE(std::isfinite(metadata.estimated_snr_db));
        for (float sample : output) EXPECT_TRUE(std::isfinite(sample));
    }
}

TEST(BedrockCalibration, NoEvidenceCannotAdvanceOrCertify) {
    apm::AutoCalibrationEngine calibration;
    calibration.start_calibration();
    calibration.advance_step();
    EXPECT_EQ(calibration.get_current_step(), apm::AutoCalibrationEngine::Step::MeasureNoiseFloor);
    EXPECT_FALSE(calibration.get_result().valid);
    apm::AudioFrame frame(10, 16000, 1);
    frame.samples()[0] = std::numeric_limits<float>::quiet_NaN();
    calibration.process_frame(frame);
    calibration.advance_step();
    EXPECT_EQ(calibration.get_current_step(), apm::AutoCalibrationEngine::Step::MeasureNoiseFloor);
    EXPECT_FALSE(calibration.get_result().valid);
}

TEST(BedrockProfiles, MalformedKnownFieldsDoNotBecomeTrustedCalibration) {
    apm::ProfileManager profiles;
    const std::string path = "bedrock-invalid-profile.cfg";
    for (const auto& value : {"nan", "inf", "-1", "1junk"}) {
        { std::ofstream file(path); file << "name=Bad\ncalibration_valid=1\nrecommended_input_gain=" << value << "\n"; }
        EXPECT_FALSE(profiles.load_profile(path).has_value()) << value;
    }
    { std::ofstream file(path); file << "name=Bad\ncalibration_valid=garbage\n"; }
    EXPECT_FALSE(profiles.load_profile(path).has_value());
    { std::ofstream file(path); file << "name=Incomplete\ncalibration_valid=1\n"; }
    EXPECT_FALSE(profiles.load_profile(path).has_value());
    std::remove(path.c_str());
}

#ifdef HAVE_LOCAL_TRANSLATION
TEST(BedrockTranslation, ConfigurationIsAnArgumentNotShellCode) {
    const auto script = std::filesystem::absolute("bedrock bridge.py");
    const auto marker = std::filesystem::absolute("bedrock-injection-marker");
    { std::ofstream file(script); file << "import json\nprint(json.dumps({'transcribed_text':'hello','translated_text':'hola','success':True}))\n"; }
    apm::LocalTranslationEngine::Config config;
    config.script_path = script.string();
    config.target_language = "es; touch " + marker.string() + "; #";
    config.use_gpu = false;
    apm::LocalTranslationEngine engine(config);
    auto result = engine.translate(std::vector<float>(160, 0.1f), 16000);
    EXPECT_FALSE(std::filesystem::exists(marker));
    EXPECT_TRUE(result.success); // The fixture can still receive arbitrary literal arguments.
    std::filesystem::remove(marker);
    std::filesystem::remove(script);
}

TEST(BedrockTranslation, MissingBridgeIsNotReadyAndFailureIsInitialized) {
    apm::LocalTranslationEngine::Config config;
    config.script_path = "bedrock-nonexistent-script.py";
    apm::LocalTranslationEngine engine(config);
    EXPECT_FALSE(engine.is_ready());
    auto result = engine.translate({}, 0);
    EXPECT_FALSE(result.success);
    EXPECT_FLOAT_EQ(result.confidence, 0.0f);
    EXPECT_FALSE(result.error_message.empty());
}

TEST(BedrockTranslation, WavByteRateDoesNotOverflowBeforeDivision) {
    const auto script = std::filesystem::absolute("bedrock-wav-bridge.py");
    { std::ofstream file(script);
      file << "import json,struct,sys\n"
              "with open(sys.argv[1], 'rb') as f: header=f.read(44)\n"
              "sample_rate,byte_rate=struct.unpack_from('<II',header,24)\n"
              "ok=sample_rate==200000000 and byte_rate==400000000\n"
              "print(json.dumps({'translated_text':'hola','success':ok}))\n";
    }
    apm::LocalTranslationEngine::Config config;
    config.script_path = script.string();
    config.use_gpu = false;
    apm::LocalTranslationEngine engine(config);
    const auto result = engine.translate(std::vector<float>(8, 0.1f), 200000000);
    EXPECT_TRUE(result.success) << result.error_message;
    std::filesystem::remove(script);
}

TEST(BedrockTranslation, MalformedEvidenceCannotBecomeSuccessfulTranslation) {
    const auto script = std::filesystem::absolute("bedrock-json-bridge.py");
    for (const auto* payload : {
        R"({"translated_text":"hola","success":truth})",
        R"({"translated_text":"hola","success":"true"})",
        R"({"translated_text":"hola","success":true} garbage)",
        R"({"translated_text":null,"success":true})",
        R"({"translated_text":"hola","success":false,"success":true})",
        R"({"translated_text":null,"translated_text":"hola","success":true})"}) {
        { std::ofstream file(script); file << "print('" << payload << "')\n"; }
        apm::LocalTranslationEngine::Config config;
        config.script_path = script.string();
        config.use_gpu = false;
        apm::LocalTranslationEngine engine(config);
        auto result = engine.translate(std::vector<float>(160, 0.1f), 16000);
        EXPECT_FALSE(result.success) << payload;
        EXPECT_FLOAT_EQ(result.confidence, 0.0f);
    }
    std::filesystem::remove(script);
}

TEST(BedrockTranslation, WhitespaceOnlyOutputDoesNotBecomeTranslationEvidence) {
    const auto script = std::filesystem::absolute("bedrock-whitespace-bridge.py");
    for (const auto* value : {"\\f\\v", "\\u2003", "\\u3000", "\\u0085", "\\u001c"}) {
        { std::ofstream file(script);
          file << "import json\nprint(json.dumps({'translated_text':'" << value << "','success':True}))\n";
        }
        apm::LocalTranslationEngine::Config config;
        config.script_path = script.string();
        config.use_gpu = false;
        apm::LocalTranslationEngine engine(config);
        const auto result = engine.translate(std::vector<float>(8, 0.1f), 16000);
        EXPECT_FALSE(result.success) << value;
        EXPECT_FLOAT_EQ(result.confidence, 0.0f) << value;
    }
    std::filesystem::remove(script);
}

TEST(BedrockTranslation, IndependentNestedKeysRemainValidEvidence) {
    const auto script = std::filesystem::absolute("bedrock-nested-bridge.py");
    { std::ofstream file(script);
      file << "import json\n"
              "print(json.dumps({'translated_text':'\\u2003hola\\u3000','success':True,"
              "'metadata':{'source':{'language':'en'},'target':{'language':'es'}}}))\n";
    }
    apm::LocalTranslationEngine::Config config;
    config.script_path = script.string();
    config.use_gpu = false;
    apm::LocalTranslationEngine engine(config);
    const auto result = engine.translate(std::vector<float>(8, 0.1f), 16000);
    EXPECT_TRUE(result.success) << result.error_message;
    EXPECT_EQ(result.translated_text, "\xe2\x80\x83hola\xe3\x80\x80");
    std::filesystem::remove(script);
}
#endif
