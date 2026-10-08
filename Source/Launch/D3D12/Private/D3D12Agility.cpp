#if defined(_WIN32)
extern "C"
{
    __declspec(dllexport) extern const unsigned int D3D12SDKVersion = 619;
    __declspec(dllexport) const char* D3D12SDKPath = ".\\D3D12\\";
}
#endif
