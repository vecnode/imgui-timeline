#!/usr/bin/env bash
set -euo pipefail

# Color codes for better UX
RED="\033[31m"
GREEN="\033[32m"
YELLOW="\033[33m"
BLUE="\033[34m"
CYAN="\033[36m"
MAGENTA="\033[35m"
RESET="\033[0m"

# Function to print colored messages
print_status() {
    local color=$1
    local message=$2
    echo -e "${color}${message}${RESET}"
}

# Function to check macOS version
check_macos_version() {
    local os_version=$(sw_vers -productVersion)
    local major_version=$(echo "$os_version" | cut -d. -f1)
    local minor_version=$(echo "$os_version" | cut -d. -f2)
    
    if [[ "$major_version" -lt 12 ]] || [[ "$major_version" -eq 12 && "$minor_version" -lt 2 ]]; then
        print_status "$RED" "Error: macOS 12.2+ required. Current version: $os_version"
        exit 1
    fi
    
    print_status "$GREEN" "macOS version check passed: $os_version"
}

# Function to detect host OS and distro
detect_host_os() {
    local kernel
    kernel="$(uname -s)"

    if [[ "$kernel" == "Darwin" ]]; then
        echo "MACOS"
        return
    fi

    if [[ "$kernel" == "Linux" ]]; then
        if [[ -f "/etc/os-release" ]]; then
            local distro_id
            distro_id="$(grep '^ID=' /etc/os-release | cut -d= -f2 | tr -d '\"' | tr '[:upper:]' '[:lower:]')"
            if [[ "$distro_id" == "ubuntu" ]]; then
                echo "UBUNTU"
                return
            fi
        fi
        echo "LINUX"
        return
    fi

    if [[ "$kernel" =~ MINGW|MSYS|CYGWIN ]] || [[ "$kernel" == "Windows_NT" ]]; then
        echo "WINDOWS"
        return
    fi

    echo "UNKNOWN"
}

cpu_cores() {
    if command -v nproc >/dev/null 2>&1; then
        nproc
    elif command -v sysctl >/dev/null 2>&1; then
        sysctl -n hw.ncpu
    else
        echo 4
    fi
}

# Function to detect Metal support
check_metal_support() {
    # Check for Metal framework availability on macOS
    if [[ "$(uname -s)" == "Darwin" ]]; then
        if [[ -d "/System/Library/Frameworks/Metal.framework" ]] || \
           [[ -d "/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk/System/Library/Frameworks/Metal.framework" ]]; then
            print_status "$GREEN" "Metal framework detected"
            return 0
        else
            print_status "$YELLOW" "Metal framework not found, falling back to OpenGL"
            return 1
        fi
    else
        print_status "$YELLOW" "Not macOS, falling back to OpenGL"
        return 1
    fi
}

# Function to auto-select best backend
auto_select_backend() {
    local host_os
    host_os="$(detect_host_os)"
    case "$host_os" in
        MACOS)
            if check_metal_support >/dev/null 2>&1; then
                echo "METAL"
            else
                echo "OPENGL"
            fi
            ;;
        UBUNTU)
            echo "VULKAN"
            ;;
        LINUX)
            echo "OPENGL"
            ;;
        WINDOWS)
            echo "OPENGL"
            ;;
        *)
            echo "OPENGL"
            ;;
    esac
}

# Function to ensure ImGui is available
ensure_imgui() {
    if [[ -f "libs/imgui/imgui.h" ]] && [[ -f "libs/imgui/imgui.cpp" ]] && [[ -f "libs/imgui/backends/imgui_impl_glfw.cpp" ]]; then
        print_status "$GREEN" "ImGui found at libs/imgui"
        return
    fi

    print_status "$YELLOW" "ImGui not found or incomplete — fetching from repository..."
    mkdir -p libs

    if ! command -v git >/dev/null 2>&1; then
        print_status "$RED" "git not available. Please install git or place Dear ImGui at libs/imgui manually."
        exit 1
    fi

    # Remove stale incomplete directory (often created by CMake warnings).
    if [[ -d "libs/imgui" ]] && [[ ! -f "libs/imgui/imgui.h" ]]; then
        rm -rf "libs/imgui"
    fi

    if [[ -d "libs/imgui/.git" ]]; then
        git -C libs/imgui fetch --depth=1 origin docking
        git -C libs/imgui checkout -f docking
        git -C libs/imgui reset --hard origin/docking
    else
        git clone --depth=1 --branch docking https://github.com/ocornut/imgui libs/imgui
    fi

    if [[ ! -f "libs/imgui/imgui.h" ]] || [[ ! -f "libs/imgui/imgui.cpp" ]] || [[ ! -f "libs/imgui/backends/imgui_impl_glfw.cpp" ]]; then
        print_status "$RED" "ImGui fetch failed: required files are still missing in libs/imgui"
        exit 1
    fi

    print_status "$GREEN" "ImGui fetched successfully"
}

# Function to clean build cache
clean_build_cache() {
    local build_dir=$1
    if [[ -d "$build_dir" ]] && [[ -f "$build_dir/CMakeCache.txt" ]]; then
        print_status "$YELLOW" "Detecting stale build cache, cleaning..."
        rm -rf "$build_dir"
        print_status "$GREEN" "Build cache cleaned"
    fi
}

# Function to build the project
build_project() {
    local backend=$1
    local build_dir=$2
    local build_type=${3:-Release}
    
    # Clean stale cache if detected
    clean_build_cache "$build_dir"
    
    print_status "$CYAN" "Configuring build with backend: $backend"
    print_status "$BLUE" "   Build directory: $build_dir"
    print_status "$BLUE" "   Build type: $build_type"
    
    cmake -S . -B "$build_dir" \
        -DBUILD_DEMO=ON \
        -DUSE_BACKEND="$backend" \
        -DCMAKE_BUILD_TYPE="$build_type" \
        -DIMGUI_TIMELINE_PEDANTIC_WARNINGS=ON
    
    print_status "$CYAN" "Building project..."
    cmake --build "$build_dir" -j"$(cpu_cores)"
    
    print_status "$GREEN" "Build completed successfully!"
}

# Function to run tests
run_tests() {
    local backend=$1
    local build_dir=$2
    
    print_status "$MAGENTA" "Testing timeline application..."
    
    case "$backend" in
        METAL)
            local app_path="./$build_dir/bin/imgui_timeline_metal"
            if [[ -f "$app_path" ]]; then
                print_status "$GREEN" "Metal app built: $app_path"
                print_status "$CYAN" "Launching Metal timeline demo..."
                print_status "$YELLOW" "   Use Ctrl+Wheel to zoom, Wheel to pan, click to select items"
                "$app_path" &
                local app_pid=$!
                print_status "$BLUE" "   App launched with PID: $app_pid"
                print_status "$YELLOW" "   Press Ctrl+C to stop the demo"
                wait $app_pid
            else
                print_status "$RED" "Metal app not found at: $app_path"
            fi
            ;;
        OPENGL)
            local app_path="./$build_dir/bin/imgui_timeline_demo"
            if [[ -f "$app_path" ]]; then
                print_status "$GREEN" "OpenGL app built: $app_path"
                print_status "$CYAN" "Launching OpenGL timeline demo..."
                print_status "$YELLOW" "   Use Ctrl+Wheel to zoom, Wheel to pan, click to select items"
                "$app_path" &
                local app_pid=$!
                print_status "$BLUE" "   App launched with PID: $app_pid"
                print_status "$YELLOW" "   Press Ctrl+C to stop the demo"
                wait $app_pid
            else
                print_status "$RED" "OpenGL app not found at: $app_path"
            fi
            ;;
        VULKAN)
            local app_path="./$build_dir/bin/imgui_timeline_vulkan"
            if [[ -f "$app_path" ]]; then
                print_status "$GREEN" "Vulkan app built: $app_path"
                print_status "$CYAN" "Launching Vulkan timeline demo..."
                print_status "$YELLOW" "   Use Ctrl+Wheel to zoom, Wheel to pan, click to select items"
                "$app_path" &
                local app_pid=$!
                print_status "$BLUE" "   App launched with PID: $app_pid"
                print_status "$YELLOW" "   Press Ctrl+C to stop the demo"
                wait $app_pid
            else
                print_status "$RED" "Vulkan app not found at: $app_path"
            fi
            ;;
        *)
            print_status "$RED" "Unsupported backend: $backend"
            ;;
    esac
}

# Function to show usage
show_usage() {
    echo -e "${CYAN}ImGui Timeline Addon - Cross-platform Build Script${RESET}"
    echo ""
    echo -e "${BLUE}Usage:${RESET}"
    echo "  $0 [OPTIONS]"
    echo ""
    echo -e "${BLUE}Options:${RESET}"
    echo "  -b, --backend BACKEND    Specify backend (AUTO, METAL, VULKAN, OPENGL)"
    echo "  -d, --debug              Build in Debug mode"
    echo "  -t, --test               Run tests after build"
    echo "  -c, --clean              Force clean build (remove build cache)"
    echo "  -h, --help               Show this help message"
    echo ""
    echo -e "${BLUE}Examples:${RESET}"
    echo "  $0                    # Auto-detect backend for this OS, Release build"
    echo "  $0 -b METAL          # Force Metal backend (macOS)"
    echo "  $0 -b VULKAN         # Force Vulkan backend (Linux/Ubuntu)"
    echo "  $0 -b OPENGL -d      # OpenGL backend, Debug build"
    echo "  $0 -t                # Build and test"
    echo "  $0 -c                # Force clean build"
    echo ""
}

# Main script
main() {
    local backend="AUTO"
    local build_type="Release"
    local run_tests_flag=false
    local force_clean=false
    
    # Parse command line arguments
    while [[ $# -gt 0 ]]; do
        case $1 in
            -b|--backend)
                backend="$2"
                shift 2
                ;;
            -d|--debug)
                build_type="Debug"
                shift
                ;;
            -t|--test)
                run_tests_flag=true
                shift
                ;;
            -c|--clean)
                force_clean=true
                shift
                ;;
            -h|--help)
                show_usage
                exit 0
                ;;
            *)
                print_status "$RED" "❌ Unknown option: $1"
                show_usage
                exit 1
                ;;
        esac
    done
    
    # Header
    echo -e "${CYAN}================================================${RESET}"
    echo -e "${CYAN}        ImGui Timeline Addon Builder${RESET}"
    echo -e "${CYAN}          Cross-platform Mode${RESET}"
    echo -e "${CYAN}================================================${RESET}"
    echo ""
    
    # System checks
    print_status "$BLUE" "System Information:"
    print_status "$BLUE" "   OS: $(uname -s) $(uname -r)"
    print_status "$BLUE" "   Architecture: $(uname -m)"
    print_status "$BLUE" "   CPU Cores: $(cpu_cores)"
    echo ""

    local host_os
    host_os="$(detect_host_os)"
    print_status "$BLUE" "   Detected Host: $host_os"

    if [[ "$host_os" == "MACOS" ]]; then
        check_macos_version
    elif [[ "$host_os" == "UBUNTU" ]]; then
        print_status "$GREEN" "Ubuntu detected: using Vulkan backend by default"
    elif [[ "$host_os" == "LINUX" ]]; then
        print_status "$GREEN" "Linux detected: using OpenGL backend by default"
    fi
    echo ""
    
    # Auto-select backend if needed
    if [[ "$backend" == "AUTO" ]]; then
        print_status "$CYAN" "Auto-detecting best backend for your system..."
        backend=$(auto_select_backend)
        print_status "$GREEN" "Selected backend: $backend"
    else
        print_status "$BLUE" "Using specified backend: $backend"
    fi
    echo ""
    
    # Ensure ImGui is available
    ensure_imgui
    echo ""
    
    # Build directory
    local build_dir="build/build_${backend}_${build_type}_$(echo "$host_os" | tr '[:upper:]' '[:lower:]')"
    
    # Force clean if requested
    if [[ "$force_clean" == true ]]; then
        print_status "$CYAN" "Force cleaning build cache..."
        rm -rf "build"
        print_status "$GREEN" "Build cache cleaned"
        echo ""
    fi
    
    # Build the project
    build_project "$backend" "$build_dir" "$build_type"
    echo ""
    
    # Show run instructions
    print_status "$GREEN" "Build completed! Run your app with:"
    case "$backend" in
        METAL)
            echo -e "   ${CYAN}./$build_dir/bin/imgui_timeline_metal${RESET}"
            ;;
        OPENGL)
            echo -e "   ${CYAN}./$build_dir/bin/imgui_timeline_demo${RESET}"
            ;;
        VULKAN)
            echo -e "   ${CYAN}./$build_dir/bin/imgui_timeline_vulkan${RESET}"
            ;;
    esac
    echo ""
    
    # Run tests if requested
    if [[ "$run_tests_flag" == true ]]; then
        echo ""
        run_tests "$backend" "$build_dir"
    fi
    
    # Final instructions
    echo ""
    print_status "$CYAN" "Development Tips:"
    print_status "$BLUE" "   • Use Ctrl+Wheel to zoom the timeline"
    print_status "$BLUE" "   • Use Wheel or middle/right-drag to pan"
    print_status "$BLUE" "   • Click to select timeline items"
    print_status "$BLUE" "   • Drag to move or resize items"
    print_status "$BLUE" "   • Press 'q' to quit the demo"
    echo ""
    print_status "$GREEN" "Happy coding with ImGui Timeline!"
}

# Run main function with all arguments
main "$@"
