#pragma once

#include <ppltasks.h>

using namespace Windows::Foundation::Collections;
using namespace Windows::Perception::Spatial;
using namespace Windows::Storage::Streams;

namespace SpatialAnchorImportExportHelper
{
	// Exports a byte buffer containing all of the anchors in the given collection.
	// This function will place data in a buffer using a std::vector<byte>.
	// The data buffer contains one or more Anchors if one or more Anchors were
	// successfully imported; otherwise, it is not modified.
	inline Concurrency::task<bool> ExportAnchorDataAsync(
		std::vector<byte>* anchorByteDataOut,
		IMap<Platform::String^, SpatialAnchor^>^ anchorsToExport)
	{
		// Create a random access stream to process the anchor byte data.
		InMemoryRandomAccessStream^ stream = ref new InMemoryRandomAccessStream();

		// Get an output stream for the anchor byte stream.
		IOutputStream^ outputStream = stream->GetOutputStreamAt(0);

		// Request access to spatial data.
		auto accessRequestedTask = Concurrency::create_task(
			SpatialAnchorTransferManager::RequestAccessAsync()).then(
				[anchorsToExport, outputStream](SpatialPerceptionAccessStatus status)
		{
			if (status == SpatialPerceptionAccessStatus::Allowed)
			{
				// Access is allowed.

				// Export the indicated set of anchors.
				return Concurrency::create_task(
					SpatialAnchorTransferManager::TryExportAnchorsAsync(
						anchorsToExport,
						outputStream));
			}
			else
			{
				// Access is denied.
				return Concurrency::task_from_result<bool>(false);
			}
		});

		// Get the input stream for the anchor byte stream.
		IInputStream^ inputStream = stream->GetInputStreamAt(0);

		// Create a DataReader, to get bytes from the anchor byte stream.
		DataReader^ reader = ref new DataReader(inputStream);
		return accessRequestedTask.then([anchorByteDataOut, stream, reader](bool anchorsExported)
		{
			if (anchorsExported)
			{
				// Get the size of the exported anchor byte stream.
				size_t bufferSize = static_cast<size_t>(stream->Size);

				// Resize the output buffer to accept the data from the stream.
				anchorByteDataOut->reserve(bufferSize);
				anchorByteDataOut->resize(bufferSize);

				// Read the exported anchor store into the stream.
				return Concurrency::create_task(reader->LoadAsync(bufferSize));
			}
			else
			{
				return Concurrency::task_from_result<size_t>(0);
			}
		})
		.then([anchorByteDataOut, reader](size_t bytesRead)
		{
			if (bytesRead > 0)
			{
				// Read the bytes from the stream, into our data output buffer.
				reader->ReadBytes(Platform::ArrayReference<byte>(&(*anchorByteDataOut)[0], bytesRead));

				return true;
			}
			else
			{
				return false;
			}
		});
	};
}
