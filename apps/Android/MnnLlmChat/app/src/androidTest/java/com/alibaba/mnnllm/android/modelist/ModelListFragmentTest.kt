package com.alibaba.mnnllm.android.modelist

import androidx.test.espresso.Espresso.onView
import androidx.test.espresso.action.ViewActions.click
import androidx.test.espresso.assertion.ViewAssertions.matches
import androidx.test.espresso.matcher.ViewMatchers.*
import androidx.test.ext.junit.rules.ActivityScenarioRule
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.filters.LargeTest
import com.alibaba.mnnllm.android.MainActivity // Assuming this is the host activity
import com.alibaba.mnnllm.android.R
import org.hamcrest.Matchers.allOf // For combining matchers
import org.hamcrest.CoreMatchers.not // Added for not(isDisplayed())
import org.junit.Rule
import org.junit.Test
import org.junit.runner.RunWith

@RunWith(AndroidJUnit4::class)
@LargeTest
class ModelListFragmentTest {

    // Rule to launch MainActivity before each test
    // Ensure MainActivity is correctly set up to show ModelListFragment,
    // or navigate to it if necessary in a @Before method.
    @get:Rule
    val activityRule = ActivityScenarioRule(MainActivity::class.java)

    // NOTE: For Espresso tests to run reliably, it's crucial to disable system animations
    // on the test device/emulator. This can be done via Developer Options.

    @Test
    fun actionBarFilterIcons_areDisplayed() {
        // Check if the Download State filter trigger icon is displayed
        // Ensure R.id.action_trigger_filter_download_state is the correct ID from menu_main.xml
        onView(withId(R.id.action_trigger_filter_download_state))
            .check(matches(isDisplayed()))

        // Check if the Modality filter trigger icon is displayed
        // Ensure R.id.action_trigger_filter_modality is the correct ID
        onView(withId(R.id.action_trigger_filter_modality))
            .check(matches(isDisplayed()))
    }

    // More tests will be added here for interaction:
    // - Clicking icons
    // - Verifying dialogs open
    // - Selecting options in dialogs
    // - Verifying RecyclerView content changes
}

    // (Existing imports and @get:Rule activityRule should be present)
    // ...

    @Test
    fun clickDownloadStateFilterIcon_opensDialog_andSelectsOption() {
        // Click the Download State filter trigger icon
        onView(withId(R.id.action_trigger_filter_download_state)).perform(click())

        // Check if the dialog is displayed - check for the dialog title
        onView(withId(R.id.dialog_filter_title)) // This ID is in dialog_filter_options.xml
            .check(matches(isDisplayed()))
        onView(withId(R.id.dialog_filter_title))
            .check(matches(withText(R.string.filter_download_state))) // Check title string

        // Click on the "Downloaded" option.
        // We need a robust way to find this. Assuming options are RadioButtons
        // and their text comes from R.string.filter_download_state_downloaded.
        // If R.id.filter_options_recyclerview contains items with RadioButtons, click one.
        // This relies on the dialog using R.layout.list_item_filter_option for its items.
        onView(allOf(withId(R.id.filter_option_radiobutton), withText(R.string.filter_download_state_downloaded)))
            .perform(click())

        // After clicking, the dialog should dismiss.
        // Verify by checking if the dialog title is no longer displayed.
        onView(withId(R.id.dialog_filter_title))
            .check(matches(not(isDisplayed()))) // Or use ViewAssertions.doesNotExist()

        // TODO: Add verification for RecyclerView content change if feasible
        // For example, if "Downloaded" results in 1 item:
        // onView(withId(R.id.model_list_recycler_view)).check(matches(hasDescendant(withText("Model One (Vision)")))) // If only "id1" is downloaded
        // Or check item count: onView(withId(R.id.model_list_recycler_view)).check(RecyclerViewItemCountAssertion(1))
        // This requires RecyclerViewItemCountAssertion or similar custom matcher.
    }

    @Test
    fun clickModalityFilterIcon_opensDialog_andSelectsOption() {
        // Click the Modality filter trigger icon
        onView(withId(R.id.action_trigger_filter_modality)).perform(click())

        // Check if the dialog is displayed - check for the dialog title
        onView(withId(R.id.dialog_filter_title))
            .check(matches(isDisplayed()))
        onView(withId(R.id.dialog_filter_title))
            .check(matches(withText(R.string.filter_modality)))

        // Click on the "Multimodal" option.
        onView(allOf(withId(R.id.filter_option_radiobutton), withText(R.string.filter_modality_multimodal)))
            .perform(click())

        // Dialog should dismiss.
        onView(withId(R.id.dialog_filter_title))
            .check(matches(not(isDisplayed())))

        // TODO: Add verification for RecyclerView content change
    }
