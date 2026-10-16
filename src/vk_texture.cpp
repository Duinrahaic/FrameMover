#include "vk_texture.h"

#include <cstring>

namespace
{
constexpr VkFormat kFormat = VK_FORMAT_R8G8B8A8_UNORM;

// Splits OpenVR's space separated extension list.
std::vector<std::string> SplitExtensions( const std::string &s )
{
	std::vector<std::string> out;
	std::string cur;
	for ( char c : s )
	{
		if ( c == ' ' )
		{
			if ( !cur.empty() )
				out.push_back( cur );
			cur.clear();
		}
		else if ( c )
			cur += c;
	}
	if ( !cur.empty() )
		out.push_back( cur );
	return out;
}

std::string VkErr( const char *what, VkResult r )
{
	return std::string( what ) + " failed (VkResult " + std::to_string( (int)r ) + ")";
}
} // namespace

VulkanContext::~VulkanContext()
{
	if ( device )
	{
		vkDeviceWaitIdle( device );
		if ( pool )
			vkDestroyCommandPool( device, pool, nullptr );
		vkDestroyDevice( device, nullptr );
	}
	if ( instance )
		vkDestroyInstance( instance, nullptr );
}

bool VulkanContext::FindMemoryType( uint32_t typeBits, VkMemoryPropertyFlags flags, uint32_t &index ) const
{
	for ( uint32_t i = 0; i < m_memProps.memoryTypeCount; i++ )
	{
		if ( ( typeBits & ( 1u << i ) ) && ( m_memProps.memoryTypes[i].propertyFlags & flags ) == flags )
		{
			index = i;
			return true;
		}
	}
	return false;
}

bool VulkanContext::Init( std::string &err )
{
	vr::IVRCompositor *compositor = vr::VRCompositor();
	std::vector<std::string> instExt, devExt;
	if ( compositor )
	{
		uint32_t n = compositor->GetVulkanInstanceExtensionsRequired( nullptr, 0 );
		if ( n > 0 )
		{
			std::string buf( n, '\0' );
			compositor->GetVulkanInstanceExtensionsRequired( buf.data(), n );
			instExt = SplitExtensions( buf );
		}
	}

	std::vector<const char *> instNames;
	for ( const std::string &e : instExt )
		instNames.push_back( e.c_str() );

	VkApplicationInfo app = { VK_STRUCTURE_TYPE_APPLICATION_INFO };
	app.pApplicationName = "passthrough-overlay";
	app.apiVersion = VK_API_VERSION_1_1;
	VkInstanceCreateInfo ici = { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
	ici.pApplicationInfo = &app;
	ici.enabledExtensionCount = (uint32_t)instNames.size();
	ici.ppEnabledExtensionNames = instNames.data();
	VkResult r = vkCreateInstance( &ici, nullptr, &instance );
	if ( r != VK_SUCCESS )
	{
		err = VkErr( "vkCreateInstance", r );
		return false;
	}

	uint32_t count = 0;
	vkEnumeratePhysicalDevices( instance, &count, nullptr );
	if ( count == 0 )
	{
		err = "no Vulkan physical devices";
		return false;
	}
	std::vector<VkPhysicalDevice> devices( count );
	vkEnumeratePhysicalDevices( instance, &count, devices.data() );

	// Use the GPU SteamVR renders on; fall back to the first one.
	uint64_t hmdDevice = 0;
	if ( vr::VRSystem() )
		vr::VRSystem()->GetOutputDevice( &hmdDevice, vr::TextureType_Vulkan, (VkInstance_T *)instance );
	physical = devices[0];
	for ( VkPhysicalDevice d : devices )
		if ( (uint64_t)d == hmdDevice )
			physical = d;
	vkGetPhysicalDeviceMemoryProperties( physical, &m_memProps );

	if ( compositor )
	{
		uint32_t n = compositor->GetVulkanDeviceExtensionsRequired( (VkPhysicalDevice_T *)physical, nullptr, 0 );
		if ( n > 0 )
		{
			std::string buf( n, '\0' );
			compositor->GetVulkanDeviceExtensionsRequired( (VkPhysicalDevice_T *)physical, buf.data(), n );
			devExt = SplitExtensions( buf );
		}
	}
	// Only enable what the driver actually offers.
	uint32_t extCount = 0;
	vkEnumerateDeviceExtensionProperties( physical, nullptr, &extCount, nullptr );
	std::vector<VkExtensionProperties> available( extCount );
	vkEnumerateDeviceExtensionProperties( physical, nullptr, &extCount, available.data() );
	std::vector<const char *> devNames;
	for ( const std::string &e : devExt )
		for ( const VkExtensionProperties &p : available )
			if ( e == p.extensionName )
			{
				devNames.push_back( e.c_str() );
				break;
			}

	uint32_t qCount = 0;
	vkGetPhysicalDeviceQueueFamilyProperties( physical, &qCount, nullptr );
	std::vector<VkQueueFamilyProperties> families( qCount );
	vkGetPhysicalDeviceQueueFamilyProperties( physical, &qCount, families.data() );
	bool found = false;
	for ( uint32_t i = 0; i < qCount; i++ )
		if ( families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT )
		{
			queueFamily = i;
			found = true;
			break;
		}
	if ( !found )
	{
		err = "no graphics queue family";
		return false;
	}

	float priority = 1.0f;
	VkDeviceQueueCreateInfo qci = { VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO };
	qci.queueFamilyIndex = queueFamily;
	qci.queueCount = 1;
	qci.pQueuePriorities = &priority;
	VkDeviceCreateInfo dci = { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
	dci.queueCreateInfoCount = 1;
	dci.pQueueCreateInfos = &qci;
	dci.enabledExtensionCount = (uint32_t)devNames.size();
	dci.ppEnabledExtensionNames = devNames.data();
	r = vkCreateDevice( physical, &dci, nullptr, &device );
	if ( r != VK_SUCCESS )
	{
		err = VkErr( "vkCreateDevice", r );
		return false;
	}
	vkGetDeviceQueue( device, queueFamily, 0, &queue );

	VkCommandPoolCreateInfo pci = { VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
	pci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	pci.queueFamilyIndex = queueFamily;
	r = vkCreateCommandPool( device, &pci, nullptr, &pool );
	if ( r != VK_SUCCESS )
	{
		err = VkErr( "vkCreateCommandPool", r );
		return false;
	}
	return true;
}

OverlayTexture::~OverlayTexture()
{
	Destroy();
}

void OverlayTexture::Destroy()
{
	VkDevice dev = m_ctx.device;
	if ( !dev )
		return;
	vkDeviceWaitIdle( dev );
	for ( Slot &s : m_slots )
	{
		if ( s.fence )
			vkDestroyFence( dev, s.fence, nullptr );
		if ( s.cmd )
			vkFreeCommandBuffers( dev, m_ctx.pool, 1, &s.cmd );
		if ( s.image )
			vkDestroyImage( dev, s.image, nullptr );
		if ( s.memory )
			vkFreeMemory( dev, s.memory, nullptr );
		s = Slot();
	}
	if ( m_stagingMapped )
		vkUnmapMemory( dev, m_stagingMemory );
	if ( m_staging )
		vkDestroyBuffer( dev, m_staging, nullptr );
	if ( m_stagingMemory )
		vkFreeMemory( dev, m_stagingMemory, nullptr );
	m_stagingMapped = nullptr;
	m_staging = VK_NULL_HANDLE;
	m_stagingMemory = VK_NULL_HANDLE;
	for ( bool &f : m_fresh )
		f = true;
	m_width = m_height = 0;
	m_next = 0;
}

bool OverlayTexture::Create( uint32_t w, uint32_t h, std::string &err )
{
	VkDevice dev = m_ctx.device;
	VkDeviceSize bytes = (VkDeviceSize)w * h * 4;

	VkBufferCreateInfo bci = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
	bci.size = bytes;
	bci.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	VkResult r = vkCreateBuffer( dev, &bci, nullptr, &m_staging );
	if ( r != VK_SUCCESS )
	{
		err = VkErr( "vkCreateBuffer", r );
		return false;
	}
	VkMemoryRequirements req;
	vkGetBufferMemoryRequirements( dev, m_staging, &req );
	VkMemoryAllocateInfo mai = { VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO };
	mai.allocationSize = req.size;
	if ( !m_ctx.FindMemoryType( req.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, mai.memoryTypeIndex ) )
	{
		err = "no host-visible coherent memory type";
		return false;
	}
	if ( ( r = vkAllocateMemory( dev, &mai, nullptr, &m_stagingMemory ) ) != VK_SUCCESS ||
		( r = vkBindBufferMemory( dev, m_staging, m_stagingMemory, 0 ) ) != VK_SUCCESS ||
		( r = vkMapMemory( dev, m_stagingMemory, 0, VK_WHOLE_SIZE, 0, &m_stagingMapped ) ) != VK_SUCCESS )
	{
		err = VkErr( "staging buffer setup", r );
		return false;
	}

	for ( Slot &s : m_slots )
	{
		VkImageCreateInfo ici = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
		ici.imageType = VK_IMAGE_TYPE_2D;
		ici.format = kFormat;
		ici.extent = { w, h, 1 };
		ici.mipLevels = 1;
		ici.arrayLayers = 1;
		ici.samples = VK_SAMPLE_COUNT_1_BIT;
		ici.tiling = VK_IMAGE_TILING_OPTIMAL;
		ici.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		if ( ( r = vkCreateImage( dev, &ici, nullptr, &s.image ) ) != VK_SUCCESS )
		{
			err = VkErr( "vkCreateImage", r );
			return false;
		}
		vkGetImageMemoryRequirements( dev, s.image, &req );
		mai.allocationSize = req.size;
		if ( !m_ctx.FindMemoryType( req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, mai.memoryTypeIndex ) )
		{
			err = "no device-local memory type";
			return false;
		}
		if ( ( r = vkAllocateMemory( dev, &mai, nullptr, &s.memory ) ) != VK_SUCCESS ||
			( r = vkBindImageMemory( dev, s.image, s.memory, 0 ) ) != VK_SUCCESS )
		{
			err = VkErr( "image memory", r );
			return false;
		}
		VkCommandBufferAllocateInfo cai = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
		cai.commandPool = m_ctx.pool;
		cai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		cai.commandBufferCount = 1;
		VkFenceCreateInfo fci = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
		if ( ( r = vkAllocateCommandBuffers( dev, &cai, &s.cmd ) ) != VK_SUCCESS ||
			( r = vkCreateFence( dev, &fci, nullptr, &s.fence ) ) != VK_SUCCESS )
		{
			err = VkErr( "command buffer/fence", r );
			return false;
		}
	}
	m_width = w;
	m_height = h;
	for ( bool &f : m_fresh )
		f = true;
	return true;
}

bool OverlayTexture::Update( vr::VROverlayHandle_t overlay, const uint8_t *rgba, uint32_t w, uint32_t h, std::string &err )
{
	if ( w == 0 || h == 0 )
	{
		err = "empty image";
		return false;
	}
	if ( w != m_width || h != m_height )
	{
		Destroy();
		if ( !Create( w, h, err ) )
		{
			Destroy();
			return false;
		}
	}

	VkDevice dev = m_ctx.device;
	int index = m_next;
	Slot &s = m_slots[index];
	m_next = ( m_next + 1 ) % kSlots;

	memcpy( m_stagingMapped, rgba, (size_t)w * h * 4 );

	vkResetCommandBuffer( s.cmd, 0 );
	VkCommandBufferBeginInfo begin = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
	begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	vkBeginCommandBuffer( s.cmd, &begin );

	VkImageMemoryBarrier toDst = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
	toDst.srcAccessMask = m_fresh[index] ? 0 : VK_ACCESS_TRANSFER_READ_BIT;
	toDst.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	toDst.oldLayout = m_fresh[index] ? VK_IMAGE_LAYOUT_UNDEFINED : VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	toDst.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	toDst.srcQueueFamilyIndex = toDst.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	toDst.image = s.image;
	toDst.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
	vkCmdPipelineBarrier( s.cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &toDst );

	VkBufferImageCopy copy = {};
	copy.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
	copy.imageExtent = { w, h, 1 };
	vkCmdCopyBufferToImage( s.cmd, m_staging, s.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy );

	// The compositor reads overlay textures as a transfer source.
	VkImageMemoryBarrier toSrc = toDst;
	toSrc.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	toSrc.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
	toSrc.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	toSrc.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	vkCmdPipelineBarrier( s.cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &toSrc );
	vkEndCommandBuffer( s.cmd );

	vkResetFences( dev, 1, &s.fence );
	VkSubmitInfo submit = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
	submit.commandBufferCount = 1;
	submit.pCommandBuffers = &s.cmd;
	VkResult r = vkQueueSubmit( m_ctx.queue, 1, &submit, s.fence );
	if ( r != VK_SUCCESS )
	{
		err = VkErr( "vkQueueSubmit", r );
		return false;
	}
	r = vkWaitForFences( dev, 1, &s.fence, VK_TRUE, 1000000000ull );
	if ( r != VK_SUCCESS )
	{
		err = VkErr( "vkWaitForFences", r );
		return false;
	}
	m_fresh[index] = false;

	vr::VRVulkanTextureData_t data = {};
	data.m_nImage = (uint64_t)s.image;
	data.m_pDevice = (VkDevice_T *)m_ctx.device;
	data.m_pPhysicalDevice = (VkPhysicalDevice_T *)m_ctx.physical;
	data.m_pInstance = (VkInstance_T *)m_ctx.instance;
	data.m_pQueue = (VkQueue_T *)m_ctx.queue;
	data.m_nQueueFamilyIndex = m_ctx.queueFamily;
	data.m_nWidth = w;
	data.m_nHeight = h;
	data.m_nFormat = kFormat;
	data.m_nSampleCount = 1;
	vr::Texture_t texture = { &data, vr::TextureType_Vulkan, vr::ColorSpace_Gamma };
	vr::EVROverlayError oe = vr::VROverlay()->SetOverlayTexture( overlay, &texture );
	if ( oe != vr::VROverlayError_None )
	{
		err = std::string( "SetOverlayTexture: " ) + vr::VROverlay()->GetOverlayErrorNameFromEnum( oe );
		return false;
	}
	return true;
}
