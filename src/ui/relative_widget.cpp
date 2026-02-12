    ImGui::Separator();

    // === FOOTER - ALWAYS DISPLAY ===
    renderFooter(relative);

    ImGui::End();
    ImGui::PopStyleVar(2);  // FIXED: Only 2 PushStyleVar calls were made (WindowPadding, ItemSpacing)
    ImGui::PopStyleColor(1);
}