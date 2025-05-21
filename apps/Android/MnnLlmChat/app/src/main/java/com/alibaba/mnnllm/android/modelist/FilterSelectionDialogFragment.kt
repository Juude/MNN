package com.alibaba.mnnllm.android.modelist

import android.app.Dialog
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.RadioButton
import android.widget.TextView
import androidx.fragment.app.DialogFragment
import androidx.fragment.app.Fragment // Added for targetFragment
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import com.alibaba.mnnllm.android.R

class FilterSelectionDialogFragment : DialogFragment() {

    interface FilterOptionListener {
        fun onOptionSelected(requestCode: Int, selectedOption: String, selectedIndex: Int)
    }

    private lateinit var titleTextView: TextView
    private lateinit var optionsRecyclerView: RecyclerView
    private lateinit var adapter: FilterOptionsAdapter

    private var dialogTitle: String = "Select Filter"
    private var options: List<String> = emptyList()
    private var selectedIndex: Int = 0
    private var requestCode: Int = 0 // To identify which filter this dialog is for

    companion object {
        const val ARG_TITLE = "title"
        const val ARG_OPTIONS = "options"
        const val ARG_SELECTED_INDEX = "selected_index"
        const val ARG_REQUEST_CODE = "request_code"

        fun newInstance(
            title: String,
            options: ArrayList<String>,
            selectedIndex: Int,
            requestCode: Int
            // listener: FilterOptionListener // Listener should be set via setFilterOptionListener or targetFragment
        ): FilterSelectionDialogFragment {
            val fragment = FilterSelectionDialogFragment()
            // fragment.setTargetFragment(listener as? Fragment, 0) // Alternative way to set listener
            val args = Bundle().apply {
                putString(ARG_TITLE, title)
                putStringArrayList(ARG_OPTIONS, options)
                putInt(ARG_SELECTED_INDEX, selectedIndex)
                putInt(ARG_REQUEST_CODE, requestCode)
            }
            fragment.arguments = args
            return fragment
        }
    }
    
    // Store listener in a member variable if not using targetFragment
    private var listener: FilterOptionListener? = null

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        arguments?.let {
            dialogTitle = it.getString(ARG_TITLE, "Select Filter")
            options = it.getStringArrayList(ARG_OPTIONS) ?: emptyList()
            selectedIndex = it.getInt(ARG_SELECTED_INDEX, 0)
            requestCode = it.getInt(ARG_REQUEST_CODE, 0)
        }
        // Try to get listener from target fragment if set
        if (targetFragment is FilterOptionListener) {
            listener = targetFragment as FilterOptionListener
        } else if (parentFragment is FilterOptionListener) {
             listener = parentFragment as FilterOptionListener
        } else if (activity is FilterOptionListener) {
            listener = activity as FilterOptionListener
        }
    }
    
    // Method to set listener programmatically if targetFragment is not preferred
    fun setFilterOptionListener(listener: FilterOptionListener) {
        this.listener = listener
    }

    override fun onCreateView(
        inflater: LayoutInflater, container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View? {
        val view = inflater.inflate(R.layout.dialog_filter_options, container, false)
        titleTextView = view.findViewById(R.id.dialog_filter_title)
        optionsRecyclerView = view.findViewById(R.id.filter_options_recyclerview)
        return view
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        titleTextView.text = dialogTitle
        adapter = FilterOptionsAdapter(options, selectedIndex) { newSelectedIndex ->
            selectedIndex = newSelectedIndex
            listener?.onOptionSelected(requestCode, options[selectedIndex], selectedIndex)
            dismiss()
        }
        optionsRecyclerView.layoutManager = LinearLayoutManager(context)
        optionsRecyclerView.adapter = adapter
    }

    override fun onCreateDialog(savedInstanceState: Bundle?): Dialog {
        val dialog = super.onCreateDialog(savedInstanceState)
        // You can customize dialog properties here if needed, like removing the default title bar
        // dialog.requestWindowFeature(Window.FEATURE_NO_TITLE)
        return dialog
    }
}

class FilterOptionsAdapter(
    private val options: List<String>,
    private var selectedIndex: Int,
    private val onOptionClicked: (Int) -> Unit
) : RecyclerView.Adapter<FilterOptionsAdapter.ViewHolder>() {

    inner class ViewHolder(view: View) : RecyclerView.ViewHolder(view) {
        val radioButton: RadioButton = view.findViewById(R.id.filter_option_radiobutton)
    }

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): ViewHolder {
        val view = LayoutInflater.from(parent.context)
            .inflate(R.layout.list_item_filter_option, parent, false)
        return ViewHolder(view)
    }

    override fun onBindViewHolder(holder: ViewHolder, position: Int) {
        holder.radioButton.text = options[position]
        holder.radioButton.isChecked = position == selectedIndex
        holder.radioButton.setOnClickListener {
            if (position != selectedIndex) {
                val previousSelectedIndex = selectedIndex
                selectedIndex = position
                notifyItemChanged(previousSelectedIndex)
                notifyItemChanged(selectedIndex) // Corrected from notifyItemChanged(selectedIndex)
                onOptionClicked(selectedIndex)
            }
        }
        // Also handle clicks on the whole item view if RadioButton doesn't cover it all
        holder.itemView.setOnClickListener {
             holder.radioButton.performClick()
        }
    }

    override fun getItemCount(): Int = options.size
}
