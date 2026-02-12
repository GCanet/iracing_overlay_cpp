// =============================================================================
// STEERING - Steering wheel with angle indicator
// =============================================================================
void TelemetryWidget::renderSteering(float width, float height) {
    ImVec2 p = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    // If we have a texture, render it
    if (m_steeringTexture != 0) {
        // FIXED: 180° rotation (horizontal + vertical flip) and maintain aspect ratio
        float targetSize = std::min(width, height);
        float xOffset = (width - targetSize) * 0.5f;
        float yOffset = (height - targetSize) * 0.5f;

        ImVec2 topLeft = ImVec2(p.x + xOffset, p.y + yOffset);
        ImVec2 bottomRight = ImVec2(topLeft.x + targetSize, topLeft.y + targetSize);

        // 180° rotation: swap both corners (horizontal + vertical flip)
        ImVec2 uv0(1.0f, 1.0f);  // Bottom-right
        ImVec2 uv1(0.0f, 0.0f);  // Top-left

        dl->AddImage((ImTextureID)(intptr_t)m_steeringTexture, topLeft, bottomRight, uv0, uv1,
                     IM_COL32(255, 255, 255, 255));
    } else {
        // Fallback: draw a simple steering wheel representation
        ImVec2 center = ImVec2(p.x + width * 0.5f, p.y + height * 0.5f);
        float radius = std::min(width, height) * 0.35f;

        // Outer circle (wheel rim)
        dl->AddCircle(center, radius, IM_COL32(150, 150, 150, 255), 12, 2.5f);

        // Inner circle
        dl->AddCircle(center, radius * 0.6f, IM_COL32(100, 100, 100, 200), 12, 1.5f);

        // Center point
        dl->AddCircleFilled(center, radius * 0.15f, IM_COL32(200, 200, 200, 255), 8);

        // Steering angle indicator (line from center)
        float angleRad = (m_steeringAngle / m_steeringAngleMax) * 3.14159f;
        float indicatorLen = radius * 0.5f;
        ImVec2 indicatorEnd = ImVec2(center.x + std::sin(angleRad) * indicatorLen,
                                     center.y - std::cos(angleRad) * indicatorLen);
        dl->AddLine(center, indicatorEnd, IM_COL32(255, 200, 0, 255), 2.0f);
    }

    ImGui::Dummy(ImVec2(width, height));
}