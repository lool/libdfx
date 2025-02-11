/***************************************************************
 * Copyright (c) 2020 Xilinx, Inc.  All rights reserved.
 * SPDX-License-Identifier: GPL-2.0
 *
 * This file is written by taking reference file from Linux kernel
 * at: tools/testing/selftests/android/ion/ionutils.c
 *
 ***************************************************************/

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <sys/mman.h>
#include "dmabuf_alloc.h"
#include "ion.h"

static int open_device (int *devfd)
{
	*devfd = open("/dev/ion", O_RDWR);
	if (*devfd < 0) {
                printf("%s: Failed to open device\n", __func__);
                return -1;
        }

	return 0;
}

static int find_dma_heap_id(int devfd, unsigned int *dma_heap_id)
{
	struct ion_heap_data heap_info[ION_HEAP_TYPE_CUSTOM];
	struct ion_heap_query heap_query;
	int ret, i, heap_flag = 0;

	memset(&heap_query, 0, sizeof(heap_query));
        heap_query.heaps = (unsigned long int)&heap_info[0];
        heap_query.cnt = ION_HEAP_TYPE_CUSTOM;

        ret = ioctl(devfd, ION_IOC_HEAP_QUERY, &heap_query);
        if (ret < 0) {
                printf("%s: ION_IOC_HEAP_QUERY: Failed\n", __func__);
		return -1;                
        }

        for (i = 0; i < heap_query.cnt; i++) {
                if (heap_info[i].type == ION_HEAP_TYPE_DMA) {
                        *dma_heap_id = heap_info[i].heap_id;
			heap_flag = 1;
                        break;
                }
        }

        if (!heap_flag) {
                printf("%s: ION_HEAP_TYPE_DMA not exists\n", __func__);
                return -1;
        }

	return 0;
}

static int alloc_dma_buffer(unsigned int dma_heap_id,
				struct dma_buffer_info *dma_data)
{
	struct ion_allocation_data alloc_data_info = {0};
	int ret;

	alloc_data_info.heap_id_mask = 1 << dma_heap_id;
        alloc_data_info.len = dma_data->dma_buflen;

        ret = ioctl(dma_data->devfd, ION_IOC_ALLOC, &alloc_data_info);
        if (ret < 0) {
                printf("%s: ION_IOC_ALLOC: Failed\n", __func__);
                return -1;
        }

        if (alloc_data_info.fd < 0 || alloc_data_info.len <= 0) {
                printf("%s: Invalid mmap data\n", __func__);
                goto err;
        }

	dma_data->dma_buffd = alloc_data_info.fd;
        dma_data->dma_buflen = alloc_data_info.len;

        /* mapp buffer for the buffer fd */
        dma_data->dma_buffer = (unsigned char *)mmap(NULL, dma_data->dma_buflen,
						 PROT_READ|PROT_WRITE,
						 MAP_SHARED,
						 dma_data->dma_buffd, 0);
        if (dma_data->dma_buffer == MAP_FAILED) {
                printf("%s: mmap: Failed\n", __func__);
                goto err;
        }

        return 0;

err:
	if (alloc_data_info.fd)
                close(alloc_data_info.fd);

	return -1;
}

int export_dma_buffer(struct dma_buffer_info *dma_data)
{
	struct ion_allocation_data alloc_data_info;
	unsigned int dma_heap_id;
	int devfd, ret;

	if (!dma_data) {
		printf("%s: Invalid input data\n", __func__);
		return -1;
	}

	ret = open_device(&devfd);
	if (ret)
		return ret;

	ret = find_dma_heap_id(devfd, &dma_heap_id);
	if (ret)
		goto err;

	dma_data->devfd = devfd;
	ret = alloc_dma_buffer(dma_heap_id, dma_data);
	if (ret)
		goto err;

	return 0;

err:
	if (devfd)
		close(devfd);

	return -1;
}

int close_dma_buffer(struct dma_buffer_info *dma_data)
{
	if (!dma_data) {
		printf("%s: Invalid input data\n", __func__);
		return -1;
	}

	munmap(dma_data->dma_buffer, dma_data->dma_buflen);

	if (dma_data->dma_buffd > 0)
		close(dma_data->dma_buffd);

	if (dma_data->devfd > 0)
		close(dma_data->devfd);

	return 0;
}
