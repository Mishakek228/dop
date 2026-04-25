#include <windows.h>
#include <projectedfslib.h>
#include <stdio.h>
#include <combaseapi.h>

static const WCHAR* VIRTUAL_FILE_NAME = L"hello.txt";
static const char FILE_CONTENT[] = "Hello from ProjFS!\n";
static const DWORD FILE_CONTENT_SIZE = (DWORD)(sizeof(FILE_CONTENT) - 1);

// Флаг: был ли уже добавлен файл в текущем перечислении
static BOOL g_fileAdded = FALSE;

HRESULT CALLBACK GetPlaceholderInfoCallback(const PRJ_CALLBACK_DATA* callbackData) {
    if (wcscmp(callbackData->FilePathName, VIRTUAL_FILE_NAME) != 0)
        return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
    PRJ_PLACEHOLDER_INFO placeholderInfo = {};
    placeholderInfo.FileBasicInfo.IsDirectory = FALSE;
    placeholderInfo.FileBasicInfo.FileSize = FILE_CONTENT_SIZE;
    placeholderInfo.FileBasicInfo.FileAttributes = FILE_ATTRIBUTE_NORMAL;
    return PrjWritePlaceholderInfo(callbackData->NamespaceVirtualizationContext, callbackData->FilePathName, &placeholderInfo, sizeof(placeholderInfo));
}

HRESULT CALLBACK GetFileDataCallback(const PRJ_CALLBACK_DATA* callbackData, UINT64 byteOffset, UINT32 length) 
{
    if (wcscmp(callbackData->FilePathName, VIRTUAL_FILE_NAME) != 0)
        return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
    if (byteOffset >= FILE_CONTENT_SIZE)
        return HRESULT_FROM_WIN32(ERROR_HANDLE_EOF);
    UINT32 bytesToWrite = length;
    if (byteOffset + bytesToWrite > FILE_CONTENT_SIZE)
        bytesToWrite = (UINT32)(FILE_CONTENT_SIZE - byteOffset);
    void* buffer = PrjAllocateAlignedBuffer(callbackData->NamespaceVirtualizationContext, bytesToWrite);
    if (!buffer) return E_OUTOFMEMORY;
    memcpy(buffer, FILE_CONTENT + byteOffset, bytesToWrite);
    HRESULT hr = PrjWriteFileData(callbackData->NamespaceVirtualizationContext, &callbackData->DataStreamId, buffer, byteOffset, bytesToWrite);
    PrjFreeAlignedBuffer(buffer);
    return hr;
}

HRESULT CALLBACK StartEnumCallback(const PRJ_CALLBACK_DATA* callbackData, const GUID* enumerationId) 
{
    // Начинаем новое перечисление — сбрасываем флаг
    g_fileAdded = FALSE;
    return S_OK;
}

HRESULT CALLBACK GetEnumCallback(const PRJ_CALLBACK_DATA* callbackData, const GUID* enumerationId, PCWSTR searchExpression, PRJ_DIR_ENTRY_BUFFER_HANDLE dirEntryBufferHandle) 
{
    // Работаем только с корнем
    if (wcslen(callbackData->FilePathName) != 0)
        return S_FALSE;

    // Если файл уже добавили в этом перечислении, больше не добавляем
    if (g_fileAdded) 
    {
        return S_FALSE;
    }

    PRJ_FILE_BASIC_INFO fileInfo = {};
    fileInfo.IsDirectory = FALSE;
    fileInfo.FileSize = FILE_CONTENT_SIZE;
    fileInfo.FileAttributes = FILE_ATTRIBUTE_NORMAL;

    HRESULT hr = PrjFillDirEntryBuffer(VIRTUAL_FILE_NAME, &fileInfo, dirEntryBufferHandle);
    if (SUCCEEDED(hr)) 
    {
        g_fileAdded = TRUE;          // помечаем, что файл добавлен
        return S_OK;                 
    }
    return hr;
}

HRESULT CALLBACK EndEnumCallback(const PRJ_CALLBACK_DATA* callbackData, const GUID* enumerationId) 
{
    return S_OK;
}

int wmain(int argc, wchar_t* argv[]) 
{
    if (argc < 2) 
    {
        wprintf(L"Usage: %s <virtual_root_path>\n", argv[0]);
        return 1;
    }
    LPCWSTR virtualRoot = argv[1];
    wprintf(L"Virtual root: %ls\n", virtualRoot);

    CreateDirectoryW(virtualRoot, NULL);

    GUID instanceId;
    HRESULT hr = CoCreateGuid(&instanceId);
    if (FAILED(hr)) { wprintf(L"CoCreateGuid failed\n"); return 1; }

    hr = PrjMarkDirectoryAsPlaceholder(virtualRoot, NULL, NULL, &instanceId);
    if (FAILED(hr)) { wprintf(L"PrjMarkDirectoryAsPlaceholder failed: 0x%08lx\n", hr); return 1; }
    wprintf(L"Directory marked as placeholder.\n");

    PRJ_CALLBACKS callbacks = {};
    callbacks.StartDirectoryEnumerationCallback = StartEnumCallback;
    callbacks.GetDirectoryEnumerationCallback = GetEnumCallback;
    callbacks.EndDirectoryEnumerationCallback = EndEnumCallback;
    callbacks.GetPlaceholderInfoCallback = GetPlaceholderInfoCallback;
    callbacks.GetFileDataCallback = GetFileDataCallback;

    PRJ_NAMESPACE_VIRTUALIZATION_CONTEXT context = nullptr;
    PRJ_STARTVIRTUALIZING_OPTIONS options = {};
    hr = PrjStartVirtualizing(virtualRoot, &callbacks, nullptr, &options, &context);
    if (FAILED(hr)) { wprintf(L"PrjStartVirtualizing failed: 0x%08lx\n", hr); return 1; }

    wprintf(L"Virtualization started at %ls. Press Enter to stop...\n", virtualRoot);
    getchar();

    PrjStopVirtualizing(context);
    return 0;
}