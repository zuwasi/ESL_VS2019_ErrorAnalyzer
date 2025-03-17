#include <iostream>
#include <fstream>
#include <sstream>
#include <regex>
#include <vector>
#include <string>
#include <filesystem>
#include <windows.h>
#include <gdiplus.h>
#include <algorithm>  // Required for std::max()

using namespace Gdiplus;
namespace fs = std::filesystem;
#pragma comment(lib, "gdiplus.lib")

// Struct to store error details
struct ErrorEntry {
    std::string file;
    int line;
    int column;
    std::string severity;
    std::string errorCode;
    std::string description;
};

// Function to initialize GDI+
void InitGDI(ULONG_PTR& token) {
    GdiplusStartupInput gdiplusStartupInput;
    GdiplusStartup(&token, &gdiplusStartupInput, NULL);
}

// Function to shutdown GDI+
void ShutdownGDI(ULONG_PTR& token) {
    GdiplusShutdown(token);
}

// Get user input for a file path
std::string getFilePath(const std::string& prompt) {
    std::string path;
    while (true) {
        std::cout << prompt;
        std::getline(std::cin, path);
        if (fs::exists(path)) {
            return path;
        }
        std::cerr << "[ERROR] Path does not exist. Please enter a valid file path.\n";
    }
}

// Generate a unique filename by adding _1, _2, etc.
std::string getUniqueFilename(const std::string& basePath) {
    std::string path = basePath;
    std::string extension;
    size_t dotPos = basePath.find_last_of('.');

    if (dotPos != std::string::npos) {
        path = basePath.substr(0, dotPos);
        extension = basePath.substr(dotPos);
    }

    int count = 1;
    std::string newFilename = basePath;
    while (fs::exists(newFilename)) {
        newFilename = path + "_" + std::to_string(count) + extension;
        count++;
    }

    return newFilename;
}

// Parse errors from the log file
std::vector<ErrorEntry> parseErrorsLog(const std::string& logFile) {
    std::ifstream log(logFile);
    std::vector<ErrorEntry> errorList;

    if (!log.is_open()) {
        std::cerr << "[ERROR] Could not open log file: " << logFile << "\n";
        return errorList;
    }

    std::regex logPattern(R"(([^\(]+)\((\d+),(\d+)\):\s+(warning|error)\s+(\w+):\s+(.+))");
    std::string line;

    while (std::getline(log, line)) {
        std::smatch match;
        if (std::regex_search(line, match, logPattern)) {
            ErrorEntry entry = {
                match[1].str(),
                std::stoi(match[2].str()),
                std::stoi(match[3].str()),
                match[4].str(),
                match[5].str(),
                match[6].str()
            };
            errorList.push_back(entry);
        }
    }

    log.close();
    return errorList;
}

// Save errors to CSV
void saveToCSV(const std::string& outputCsv, const std::vector<ErrorEntry>& errorList) {
    std::string uniqueCsvPath = getUniqueFilename(outputCsv);
    std::ofstream csv(uniqueCsvPath);
    if (!csv.is_open()) {
        std::cerr << "[ERROR] Could not create CSV file: " << uniqueCsvPath << "\n";
        return;
    }

    csv << "File,Line,Column,Severity,Error Code,Description\n";
    for (const auto& entry : errorList) {
        csv << entry.file << "," << entry.line << "," << entry.column << ","
            << entry.severity << "," << entry.errorCode << ",\"" << entry.description << "\"\n";
    }

    csv.close();
    std::cout << "[SUCCESS] Errors saved to " << uniqueCsvPath << "\n";
}

// Convert std::string to std::wstring for GDI+
std::wstring stringToWstring(const std::string& str) {
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, NULL, 0);
    std::wstring wstr(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &wstr[0], size_needed);
    return wstr;
}

// Get the encoder CLSID for PNG
int GetEncoderClsid(const WCHAR* format, CLSID* pClsid) {
    UINT num = 0, size = 0;
    GetImageEncodersSize(&num, &size);
    if (size == 0) return -1;

    ImageCodecInfo* pImageCodecInfo = (ImageCodecInfo*)(malloc(size));
    if (!pImageCodecInfo) return -1;

    GetImageEncoders(num, size, pImageCodecInfo);
    for (UINT j = 0; j < num; ++j) {
        if (wcscmp(pImageCodecInfo[j].MimeType, format) == 0) {
            *pClsid = pImageCodecInfo[j].Clsid;
            free(pImageCodecInfo);
            return j;
        }
    }

    free(pImageCodecInfo);
    return -1;
}

// Generate a bar chart using GDI+
void generateVisualization(int errorCount, int warningCount, const std::string& outputPath) {
    ULONG_PTR gdiplusToken;
    InitGDI(gdiplusToken);

    int width = 600, height = 400;
    Bitmap bitmap(width, height, PixelFormat32bppARGB);
    Graphics graphics(&bitmap);
    graphics.Clear(Color::White);

    FontFamily fontFamily(L"Arial");
    Font titleFont(&fontFamily, 16, FontStyleBold, UnitPixel);
    Font labelFont(&fontFamily, 12, FontStyleRegular, UnitPixel);
    SolidBrush blackBrush(Color(0, 0, 0));
    SolidBrush redBrush(Color(220, 50, 50));
    SolidBrush yellowBrush(Color(220, 220, 50));

    // Draw title
    graphics.DrawString(L"Error vs Warning Count", -1, &titleFont, PointF(200, 30), &blackBrush);

    // Bar chart values
    int maxCount = (std::max)(errorCount, warningCount);
    float scale = (maxCount > 0) ? 150.0f / maxCount : 1.0f;
    int errorHeight = static_cast<int>(errorCount * scale);
    int warningHeight = static_cast<int>(warningCount * scale);

    // Draw bars
    graphics.FillRectangle(&redBrush, 150, 250 - errorHeight, 80, errorHeight);
    graphics.FillRectangle(&yellowBrush, 350, 250 - warningHeight, 80, warningHeight);

    // Draw labels
    graphics.DrawString(L"Errors", -1, &labelFont, PointF(170, 270), &blackBrush);
    graphics.DrawString(L"Warnings", -1, &labelFont, PointF(370, 270), &blackBrush);

    // Save to PNG
    std::string pngPath = getUniqueFilename(outputPath + "\\error_report.png");
    std::wstring wPngPath = stringToWstring(pngPath);
    CLSID pngClsid;
    GetEncoderClsid(L"image/png", &pngClsid);
    bitmap.Save(wPngPath.c_str(), &pngClsid, NULL);

    ShutdownGDI(gdiplusToken);
    std::cout << "[SUCCESS] Visualization saved as " << pngPath << "\n";
}

int main() {
    SetConsoleOutputCP(CP_UTF8);

    // Get paths from user
    std::string logFilePath = getFilePath("Enter the full path to errors.log: ");
    std::string outputPath = getFilePath("Enter the output folder path: ");

    // Process log file
    auto errors = parseErrorsLog(logFilePath);
    saveToCSV(outputPath + "\\errors_report.csv", errors);

    // Count errors and warnings
    int errorCount = 0, warningCount = 0;
    for (const auto& entry : errors) {
        if (entry.severity == "error") errorCount++;
        else if (entry.severity == "warning") warningCount++;
    }

    // Generate visualization
    generateVisualization(errorCount, warningCount, outputPath);

    std::cout << "[SUCCESS] Process completed successfully!\n";
    return 0;
}
