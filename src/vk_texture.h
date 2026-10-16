#pragma once
// GPU texture path for overlays. IVROverlay::SetOverlayRaw makes an overlay vanish for a couple
// of frames on every call (SteamVR reloads the image), so anything that changes must be handed
// over as a Vulkan texture with SetOverlayTexture instead.
#include <openvr.h>
#include <vulkan/vulkan.h>

#include <cstdint>
#include <string>
#include <vector>

class VulkanContext
{
public:
	VulkanContext() = default;
	~VulkanContext();
	VulkanContext( const VulkanContext & ) = delete;
	VulkanContext &operator=( const VulkanContext & ) = delete;

	// Creates an instance/device on the GPU SteamVR renders with. Needs VR_Init to have succeeded.
	bool Init( std::string &err );
	bool FindMemoryType( uint32_t typeBits, VkMemoryPropertyFlags flags, uint32_t &index ) const;

	VkInstance instance = VK_NULL_HANDLE;
	VkPhysicalDevice physical = VK_NULL_HANDLE;
	VkDevice device = VK_NULL_HANDLE;
	VkQueue queue = VK_NULL_HANDLE;
	uint32_t queueFamily = 0;
	VkCommandPool pool = VK_NULL_HANDLE;

private:
	VkPhysicalDeviceMemoryProperties m_memProps = {};
};

// One overlay's pixels on the GPU: a small ring of images so the compositor can keep reading
// the previous frame while the next one is written. All calls must come from one thread.
class OverlayTexture
{
public:
	explicit OverlayTexture( VulkanContext &ctx ) : m_ctx( ctx ) {}
	~OverlayTexture();
	OverlayTexture( const OverlayTexture & ) = delete;
	OverlayTexture &operator=( const OverlayTexture & ) = delete;

	// Uploads tightly packed RGBA8 (w*h*4 bytes) and points `overlay` at it. Resizes on demand.
	bool Update( vr::VROverlayHandle_t overlay, const uint8_t *rgba, uint32_t w, uint32_t h, std::string &err );

private:
	static constexpr int kSlots = 3;
	struct Slot
	{
		VkImage image = VK_NULL_HANDLE;
		VkDeviceMemory memory = VK_NULL_HANDLE;
		VkCommandBuffer cmd = VK_NULL_HANDLE;
		VkFence fence = VK_NULL_HANDLE;
	};

	bool Create( uint32_t w, uint32_t h, std::string &err );
	void Destroy();

	VulkanContext &m_ctx;
	Slot m_slots[kSlots];
	VkBuffer m_staging = VK_NULL_HANDLE;
	VkDeviceMemory m_stagingMemory = VK_NULL_HANDLE;
	void *m_stagingMapped = nullptr;
	uint32_t m_width = 0, m_height = 0;
	int m_next = 0;
	bool m_fresh[kSlots] = {};
};
