#include <PlaylistCWrapper.hpp>

#include <gtest/gtest.h>

namespace {

void presetSwitched(bool, uint32_t, void* userData)
{
    ++*static_cast<int*>(userData);
}

void presetFailed(const char*, const char*, void* userData)
{
    ++*static_cast<int*>(userData);
}

} // namespace

TEST(projectMPlaylistCWrapper, SuccessfulPresetAfterPreviousFailure)
{
    // ProjectMAPIMocks.cpp deliberately accepts the load without an error.
    // This verifies a previous failure cannot poison the next call.
    libprojectM::Playlist::PlaylistCWrapper playlist(nullptr);
    ASSERT_TRUE(playlist.AddItem("/valid.milk",
                                 libprojectM::Playlist::Playlist::InsertAtEnd, false));
    playlist.SetRetryCount(1);

    int switchedCount = 0;
    int failedCount = 0;
    playlist.SetPresetSwitchedCallback(&presetSwitched, &switchedCount);
    playlist.SetPresetSwitchFailedCallback(&presetFailed, &failedCount);

    // Simulate a prior load failure reported by libprojectM.
    libprojectM::Playlist::PlaylistCWrapper::OnPresetSwitchFailed(
        "/broken.milk", "Invalid preset", &playlist);

    // The next load succeeds. A stale m_lastPresetSwitchFailed flag previously
    // caused this to be treated as another error at retry count 1.
    playlist.PlayPresetIndex(0, true, true);

    EXPECT_EQ(switchedCount, 1);
    EXPECT_EQ(failedCount, 0);
}
